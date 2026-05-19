from fastapi import FastAPI, HTTPException, status
from fastapi.staticfiles import StaticFiles
from fastapi.responses import FileResponse
import os
from pathlib import Path

from .schemas import (
    BlogPostCreate,
    BlogPostUpdate,
    BlogPostPublish,
    BlogPostResponse,
    RenderRequest,
    RenderResponse,
)
from .storage import BlogStorage
from .blog_renderer import BlogRenderer

# Initialize FastAPI app
app = FastAPI(
    title="FujiNet Blog API",
    description="API for managing blog posts on Apple IIc via FujiNet",
    version="0.1.0",
)

# Initialize storage
storage = BlogStorage()
renderer = BlogRenderer()


# ============================================================================
# BLOG POST ENDPOINTS
# ============================================================================


@app.post("/api/posts", response_model=BlogPostResponse, status_code=status.HTTP_201_CREATED)
def create_post(post: BlogPostCreate) -> BlogPostResponse:
    """Create a new blog post."""
    created_post = storage.create_post(
        title=post.title,
        markdown_body=post.markdown_body,
        categories=post.categories,
        published=post.published,
    )
    html = renderer.render_markdown(created_post.markdown_body)
    return BlogPostResponse(
        id=created_post.id,
        title=created_post.title,
        slug=created_post.slug,
        markdown_body=created_post.markdown_body,
        html_body=html,
        categories=created_post.categories,
        published=created_post.published,
        created_at=created_post.created_at,
        updated_at=created_post.updated_at,
    )


@app.get("/api/posts", response_model=list[BlogPostResponse])
def list_posts(published_only: bool = False) -> list[BlogPostResponse]:
    """List all posts (or published posts only)."""
    posts = storage.list_posts(published_only=published_only)
    result = []
    for post in posts:
        html = renderer.render_markdown(post.markdown_body)
        result.append(
            BlogPostResponse(
                id=post.id,
                title=post.title,
                slug=post.slug,
                markdown_body=post.markdown_body,
                html_body=html,
                categories=post.categories,
                published=post.published,
                created_at=post.created_at,
                updated_at=post.updated_at,
            )
        )
    return result


@app.get("/api/posts/{post_id}", response_model=BlogPostResponse)
def get_post(post_id: str) -> BlogPostResponse:
    """Retrieve a single post by ID."""
    post = storage.get_post(post_id)
    if not post:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Post {post_id} not found",
        )
    html = renderer.render_markdown(post.markdown_body)
    return BlogPostResponse(
        id=post.id,
        title=post.title,
        slug=post.slug,
        markdown_body=post.markdown_body,
        html_body=html,
        categories=post.categories,
        published=post.published,
        created_at=post.created_at,
        updated_at=post.updated_at,
    )


@app.put("/api/posts/{post_id}", response_model=BlogPostResponse)
def update_post(post_id: str, post: BlogPostUpdate) -> BlogPostResponse:
    """Update a blog post."""
    updated_post = storage.update_post(
        post_id=post_id,
        title=post.title,
        markdown_body=post.markdown_body,
        categories=post.categories,
        published=post.published,
    )
    if not updated_post:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Post {post_id} not found",
        )
    html = renderer.render_markdown(updated_post.markdown_body)
    return BlogPostResponse(
        id=updated_post.id,
        title=updated_post.title,
        slug=updated_post.slug,
        markdown_body=updated_post.markdown_body,
        html_body=html,
        categories=updated_post.categories,
        published=updated_post.published,
        created_at=updated_post.created_at,
        updated_at=updated_post.updated_at,
    )


@app.patch("/api/posts/{post_id}/publish", response_model=BlogPostResponse)
def toggle_publish(post_id: str, publish: BlogPostPublish) -> BlogPostResponse:
    """Toggle the published state of a post."""
    updated_post = storage.update_post(post_id=post_id, published=publish.published)
    if not updated_post:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Post {post_id} not found",
        )
    html = renderer.render_markdown(updated_post.markdown_body)
    return BlogPostResponse(
        id=updated_post.id,
        title=updated_post.title,
        slug=updated_post.slug,
        markdown_body=updated_post.markdown_body,
        html_body=html,
        categories=updated_post.categories,
        published=updated_post.published,
        created_at=updated_post.created_at,
        updated_at=updated_post.updated_at,
    )


@app.delete("/api/posts/{post_id}", status_code=status.HTTP_204_NO_CONTENT)
def delete_post(post_id: str) -> None:
    """Delete a blog post."""
    deleted = storage.delete_post(post_id)
    if not deleted:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Post {post_id} not found",
        )


# ============================================================================
# UTILITY ENDPOINTS
# ============================================================================


@app.post("/api/render", response_model=RenderResponse)
def render_markdown(request: RenderRequest) -> RenderResponse:
    """Render Markdown to HTML."""
    html = renderer.render_markdown(request.markdown_body)
    return RenderResponse(html=html)


# ============================================================================
# STATIC FILE SERVING
# ============================================================================


@app.get("/api/posts/published", response_model=list[BlogPostResponse])
def list_published_posts() -> list[BlogPostResponse]:
    """Convenience endpoint: list published posts only."""
    posts = storage.list_posts(published_only=True)
    result = []
    for post in posts:
        html = renderer.render_markdown(post.markdown_body)
        result.append(
            BlogPostResponse(
                id=post.id,
                title=post.title,
                slug=post.slug,
                markdown_body=post.markdown_body,
                html_body=html,
                categories=post.categories,
                published=post.published,
                created_at=post.created_at,
                updated_at=post.updated_at,
            )
        )
    return result


# Serve React frontend as static assets
frontend_dist = Path(__file__).parent.parent / "frontend" / "dist"
if frontend_dist.exists():
    app.mount("/", StaticFiles(directory=str(frontend_dist), html=True), name="static")


@app.get("/")
def read_root():
    """Serve the React app or a simple welcome message."""
    index_path = frontend_dist / "index.html"
    if index_path.exists():
        return FileResponse(str(index_path))
    return {"message": "FujiNet Blog API is running. Build the React frontend with 'npm run build' in the frontend/ directory."}
