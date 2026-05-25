import hashlib
import time

from fastapi import FastAPI, HTTPException, Query, Request, status
from fastapi.responses import JSONResponse
from fastapi.staticfiles import StaticFiles
from fastapi.responses import FileResponse
import json
import os
from pathlib import Path

from .schemas import (
    BlogPostCreate,
    BlogPostUpdate,
    BlogPostPublish,
    BlogPostResponse,
    BlogPostSummary,
    BlogPostMarkdown,
    RenderRequest,
    RenderResponse,
    StatsResponse,
)
from .storage import BlogStorage
from .blog_renderer import BlogRenderer

app = FastAPI(
    title="FujiNet Blog API",
    description="API for managing blog posts on Apple IIc via FujiNet",
    version="0.1.0",
)

storage = BlogStorage()
renderer = BlogRenderer()

# ---------------------------------------------------------------------------
# Deduplication caches for PUT endpoints
#
# The FujiNet IWM firmware (Apple IIc) keeps the PUT body in a postData
# buffer.  network_json_parse() sends the HTTP PUT and reads the response,
# but the buffer is NOT cleared afterward.  When network_close() is called
# next, the firmware flushes the same buffer again — sending a second
# identical PUT to the server.
#
# CREATE: not idempotent — second flush would produce a duplicate post.
#   Fix: cache (title, body) fingerprint; return the already-created post
#   if the same fingerprint arrives again within the TTL.
#
# DELETE: second flush hits a 404 because the post is already gone.
#   Fix: cache recently-deleted IDs; return a synthetic success response
#   {"id": post_id} for any duplicate flush within the TTL.
# ---------------------------------------------------------------------------
_PUT_DEDUP_TTL = 30          # seconds — covers any realistic firmware retry
_put_dedup_cache: dict = {}  # fingerprint -> (post_id, expires_at)
_delete_dedup_cache: dict = {}  # post_id -> expires_at


def _summary(post) -> BlogPostSummary:
    return BlogPostSummary(
        id=post.id,
        title=post.title,
        slug=post.slug,
        category=post.category,
        published=post.published,
        created_at=post.created_at,
        updated_at=post.updated_at,
    )


def _response(post) -> BlogPostResponse:
    return BlogPostResponse(
        id=post.id,
        title=post.title,
        slug=post.slug,
        markdown_body=post.markdown_body,
        html_body=renderer.render_markdown(post.markdown_body),
        category=post.category,
        published=post.published,
        created_at=post.created_at,
        updated_at=post.updated_at,
    )


# ============================================================================
# BLOG POST ENDPOINTS
# ============================================================================


@app.post("/api/posts", response_model=BlogPostSummary, status_code=status.HTTP_201_CREATED)
def create_post(post: BlogPostCreate) -> BlogPostSummary:
    """Create a new blog post."""
    return _summary(storage.create_post(
        title=post.title,
        markdown_body=post.markdown_body,
        category=post.category,
        published=post.published,
    ))


@app.put("/api/posts", response_model=BlogPostSummary, status_code=status.HTTP_201_CREATED)
async def create_post_via_put(request: Request) -> BlogPostSummary:
    """Create a post via PUT — workaround for Apple IIc FujiNet IWM firmware bug where
    HTTP_CHAN_MODE_POST_SET_DATA is never applied, so POST body writes are discarded.
    PUT method writes in DATA mode (mode 0) correctly land in postData.

    Includes deduplication: the IWM firmware re-flushes the PUT body when
    network_close() is called after network_json_parse(), so the server
    receives two identical requests per logical create.  We fingerprint the
    (title, markdown_body) pair and return the already-created post for any
    duplicate that arrives within _PUT_DEDUP_TTL seconds.
    """
    body = await request.body()
    try:
        data = json.loads(body)
    except (json.JSONDecodeError, ValueError):
        raise HTTPException(
            status_code=status.HTTP_400_BAD_REQUEST, detail="Invalid JSON body"
        )
    title = data.get("title", "")
    markdown_body = data.get("markdown_body", "")
    if not title or not markdown_body:
        raise HTTPException(
            status_code=status.HTTP_422_UNPROCESSABLE_ENTITY,
            detail="title and markdown_body are required",
        )

    # Dedup: same fingerprint within TTL → return existing post
    fingerprint = hashlib.sha1(
        f"{title}\n{markdown_body}".encode()
    ).hexdigest()
    now = time.monotonic()
    cached = _put_dedup_cache.get(fingerprint)
    if cached:
        cached_id, expires_at = cached
        if now < expires_at:
            existing = storage.get_post(cached_id)
            if existing:
                return _summary(existing)
    # Evict stale entries (keep the cache from growing indefinitely)
    expired = [k for k, (_, exp) in _put_dedup_cache.items() if now >= exp]
    for k in expired:
        del _put_dedup_cache[k]

    post = storage.create_post(
        title=title,
        markdown_body=markdown_body,
        category=data.get("category", ""),
        published=bool(data.get("published", False)),
    )
    _put_dedup_cache[fingerprint] = (post.id, now + _PUT_DEDUP_TTL)
    return _summary(post)


@app.get("/api/categories")
def list_categories(published_only: bool = False):
    """List all unique categories with post counts.

    Returns [{name, count}] sorted by count desc, then name asc.
    Pass ?published_only=true for the public-facing category sidebar.
    """
    return storage.list_categories(published_only=published_only)


@app.get("/api/posts", response_model=list[BlogPostResponse])
def list_posts(
    published_only: bool = False,
    category: str = Query(default=None, description="Filter by category name"),
) -> list[BlogPostResponse]:
    """List all posts (or published posts only), optionally filtered by category."""
    return [
        _response(p)
        for p in storage.list_posts(published_only=published_only, category=category)
    ]


@app.get("/api/posts/summaries", response_model=list[BlogPostSummary])
def list_post_summaries(
    published_only: bool = False,
    category: str = Query(default=None, description="Filter by category name"),
) -> list[BlogPostSummary]:
    """Compact post list without body fields. Sized for FujiNet's receive buffer."""
    return [
        _summary(p)
        for p in storage.list_posts(published_only=published_only, category=category)
    ]


@app.get("/api/posts/published", response_model=list[BlogPostResponse])
def list_published_posts(
    category: str = Query(default=None, description="Filter by category name"),
) -> list[BlogPostResponse]:
    """Convenience endpoint: list published posts only, optionally by category."""
    return [
        _response(p)
        for p in storage.list_posts(published_only=True, category=category)
    ]


@app.get("/api/posts/slug/{slug}", response_model=BlogPostResponse)
def get_post_by_slug(slug: str) -> BlogPostResponse:
    """Fetch a published post by its URL slug (for the public blog)."""
    post = storage.get_post_by_slug(slug)
    if not post or not post.published:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Post '{slug}' not found",
        )
    return _response(post)


@app.get("/api/posts/{post_id}/markdown", response_model=BlogPostMarkdown)
def get_post_markdown(post_id: str) -> BlogPostMarkdown:
    """Return title + category + markdown_body for a post. Avoids html_body doubling the payload."""
    post = storage.get_post(post_id)
    if not post:
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail=f"Post {post_id} not found")
    return BlogPostMarkdown(title=post.title, category=post.category, markdown_body=post.markdown_body)


@app.get("/api/posts/{post_id}", response_model=BlogPostResponse)
def get_post(post_id: str) -> BlogPostResponse:
    """Retrieve a single post by ID."""
    post = storage.get_post(post_id)
    if not post:
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail=f"Post {post_id} not found")
    return _response(post)


@app.put("/api/posts/{post_id}", response_model=BlogPostSummary)
async def update_post(post_id: str, request: Request) -> BlogPostSummary:
    """Update a blog post. Accepts raw JSON body without requiring Content-Type header
    so the FujiNet IWM client can use OPEN_MODE_HTTP_PUT + network_write()."""
    body = await request.body()
    try:
        data = json.loads(body)
    except (json.JSONDecodeError, ValueError):
        raise HTTPException(status_code=status.HTTP_400_BAD_REQUEST, detail="Invalid JSON body")
    updated = storage.update_post(
        post_id=post_id,
        title=data.get("title"),
        markdown_body=data.get("markdown_body"),
        category=data.get("category"),
        published=data.get("published"),
    )
    if not updated:
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail=f"Post {post_id} not found")
    return _summary(updated)


@app.patch("/api/posts/{post_id}/publish", response_model=BlogPostSummary)
def toggle_publish(post_id: str, publish: BlogPostPublish) -> BlogPostSummary:
    """Toggle the published state of a post."""
    updated = storage.update_post(post_id=post_id, published=publish.published)
    if not updated:
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail=f"Post {post_id} not found")
    return _summary(updated)


@app.put("/api/posts/{post_id}/publish", response_model=BlogPostSummary)
async def toggle_publish_via_put(post_id: str, request: Request) -> BlogPostSummary:
    """Toggle publish via PUT — FujiNet IWM workaround (mirrors PATCH endpoint).
    Accepts raw JSON body without requiring Content-Type header."""
    body = await request.body()
    try:
        data = json.loads(body)
    except (json.JSONDecodeError, ValueError):
        raise HTTPException(status_code=status.HTTP_400_BAD_REQUEST, detail="Invalid JSON body")
    published = data.get("published")
    if published is None:
        raise HTTPException(status_code=status.HTTP_422_UNPROCESSABLE_ENTITY, detail="published is required")
    updated = storage.update_post(post_id=post_id, published=bool(published))
    if not updated:
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail=f"Post {post_id} not found")
    return _summary(updated)


@app.delete("/api/posts/{post_id}", status_code=status.HTTP_204_NO_CONTENT)
def delete_post(post_id: str) -> None:
    """Delete a blog post."""
    if not storage.delete_post(post_id):
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail=f"Post {post_id} not found")


@app.put("/api/posts/{post_id}/delete")
async def delete_post_via_put(post_id: str) -> dict:
    """Delete via PUT — FujiNet IWM workaround.

    The IWM firmware cannot send a DELETE request body, and OPEN_MODE_HTTP_DELETE
    behaviour on IWM is untested.  Using PUT + network_write() (the same workaround
    as /publish) guarantees the request reaches the server.  Returns {"id": post_id}
    so the client can confirm success with network_json_query("/id", ...).

    Includes deduplication: network_close() re-flushes the PUT body after
    network_json_parse(), so the server receives two identical requests per
    logical delete.  The second arrives after the post is already gone and
    would return 404.  We cache recently-deleted IDs for _PUT_DEDUP_TTL
    seconds and return a synthetic success for the duplicate flush.
    """
    now = time.monotonic()

    # Duplicate flush from firmware — post already deleted, return cached success
    expires_at = _delete_dedup_cache.get(post_id)
    if expires_at and now < expires_at:
        return {"id": post_id}

    # Evict stale entries
    stale = [k for k, exp in _delete_dedup_cache.items() if now >= exp]
    for k in stale:
        del _delete_dedup_cache[k]

    if not storage.delete_post(post_id):
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail=f"Post {post_id} not found")

    _delete_dedup_cache[post_id] = now + _PUT_DEDUP_TTL
    return {"id": post_id}


# ============================================================================
# STATS ENDPOINT
# ============================================================================


@app.get("/api/stats", response_model=StatsResponse)
def get_stats() -> StatsResponse:
    """Return blog statistics: post count, category breakdown,
    posts-per-month histogram for the current year, and average file size."""
    return StatsResponse(**storage.get_stats())


# ============================================================================
# UTILITY ENDPOINTS
# ============================================================================


@app.get("/api/ping")
def ping():
    """Lightweight connectivity check — useful for FujiNet client diagnostics."""
    return {"ok": True}


@app.post("/api/render", response_model=RenderResponse)
def render_markdown(request: RenderRequest) -> RenderResponse:
    """Render Markdown to HTML."""
    return RenderResponse(html=renderer.render_markdown(request.markdown_body))


# ============================================================================
# STATIC FILE SERVING  (SPA catch-all — must stay LAST)
# ============================================================================

frontend_dist = Path(__file__).parent.parent / "frontend" / "dist"

# Mount /assets separately so Starlette serves JS/CSS with proper caching
# headers without hitting the Python catch-all on every page load.
_assets_dir = frontend_dist / "assets"
if _assets_dir.exists():
    app.mount("/assets", StaticFiles(directory=str(_assets_dir)), name="assets")


@app.get("/{full_path:path}")
async def serve_spa(full_path: str) -> FileResponse:
    """Catch-all: serve any static file that exists, otherwise index.html.

    This lets React Router handle all client-side routes (/, /:category,
    /post/:slug, /admin) even on a hard reload or direct URL navigation.
    """
    if frontend_dist.exists():
        candidate = frontend_dist / full_path
        if candidate.is_file():
            return FileResponse(str(candidate))
        index_path = frontend_dist / "index.html"
        if index_path.exists():
            return FileResponse(str(index_path))
    return JSONResponse(  # type: ignore[return-value]
        {"message": "Frontend not built — run 'npm run build' in server/frontend/"}
    )
