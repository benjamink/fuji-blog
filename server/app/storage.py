import json
import os
import re
import uuid as uuid_mod
import yaml
from typing import List, Optional
from datetime import datetime
from .models import BlogPost

UNCATEGORIZED = "uncategorized"


def _category_dirname(category: str) -> str:
    """Normalise a category name to a safe directory name."""
    if not category or not category.strip():
        return UNCATEGORIZED
    s = category.lower().strip()
    s = re.sub(r"[^\w\s-]", "", s)
    s = re.sub(r"[-\s]+", "-", s)
    return s.strip("-") or UNCATEGORIZED


def _parse_md_file(path: str):
    """Return (meta_dict, body_str) from a markdown file with YAML frontmatter.

    The body is returned byte-exact: _write_md_file always inserts exactly one
    blank separator line between the frontmatter and the body, which we strip
    back off here.  This exact round-trip is required by the chunked-append
    upload path, which does repeated read-modify-write cycles."""
    with open(path, "r", encoding="utf-8") as f:
        text = f.read()
    if text.startswith("---\n"):
        parts = text.split("---\n", 2)
        if len(parts) == 3:
            meta = yaml.safe_load(parts[1]) or {}
            body = parts[2]
            if body.startswith("\n"):
                body = body[1:]   # remove the single separator line
            return meta, body
    return {}, text


def _write_md_file(path: str, meta: dict, body: str) -> None:
    """Write a markdown file with YAML frontmatter.

    Exactly one blank separator line is written between the frontmatter and the
    body, and the body is written verbatim (no trailing newline forced) so that
    _parse_md_file can reproduce it byte-for-byte."""
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        f.write("---\n")
        yaml.dump(meta, f, default_flow_style=False, allow_unicode=True, sort_keys=False)
        f.write("---\n\n")
        f.write(body)


class BlogStorage:
    """File-based persistence — posts/{category}/{slug}.md"""

    def __init__(self, data_dir: str = None):
        if data_dir is None:
            data_dir = os.environ.get("DATA_DIR") or os.path.join(
                os.path.dirname(__file__), "..", "data"
            )
        self.data_dir = data_dir
        self.posts_dir = os.path.join(data_dir, "posts")
        os.makedirs(self.posts_dir, exist_ok=True)
        self._migrate_from_json()
        self._migrate_flat_md_files()
        self._migrate_slug_mismatches()

    # -------------------------------------------------------------------------
    # Internal helpers
    # -------------------------------------------------------------------------

    def _unique_path(self, category: str, slug: str) -> str:
        """Return a non-conflicting path for posts/{category}/{slug}.md."""
        cat_dir = os.path.join(self.posts_dir, _category_dirname(category))
        os.makedirs(cat_dir, exist_ok=True)
        path = os.path.join(cat_dir, f"{slug}.md")
        if not os.path.exists(path):
            return path
        i = 2
        while True:
            path = os.path.join(cat_dir, f"{slug}-{i}.md")
            if not os.path.exists(path):
                return path
            i += 1

    def _find_path_by_id(self, post_id: str) -> Optional[str]:
        """Walk the posts tree and return the path whose frontmatter id matches."""
        for dirpath, _, filenames in os.walk(self.posts_dir):
            for fname in filenames:
                if not fname.endswith(".md"):
                    continue
                path = os.path.join(dirpath, fname)
                try:
                    meta, _ = _parse_md_file(path)
                    if meta.get("id") == post_id:
                        return path
                except Exception:
                    continue
        return None

    def _write_post(self, post: BlogPost, path: str) -> None:
        meta = {
            "id": post.id,
            "title": post.title,
            "slug": post.slug,
            "category": post.category,
            "published": post.published,
            "created_at": post.created_at.isoformat(),
            "updated_at": post.updated_at.isoformat(),
        }
        _write_md_file(path, meta, post.markdown_body)

    def _read_post(self, path: str) -> Optional[BlogPost]:
        try:
            meta, body = _parse_md_file(path)
            # Support legacy files that still have a 'categories' list
            category = meta.get("category")
            if category is None:
                cats = meta.get("categories") or []
                category = cats[0] if cats else ""
            return BlogPost(
                id=meta["id"],
                title=meta["title"],
                slug=meta["slug"],
                markdown_body=body,
                category=str(category),
                published=bool(meta.get("published", False)),
                created_at=datetime.fromisoformat(str(meta["created_at"])),
                updated_at=datetime.fromisoformat(str(meta["updated_at"])),
            )
        except (KeyError, ValueError, TypeError):
            return None

    # -------------------------------------------------------------------------
    # Migrations
    # -------------------------------------------------------------------------

    def _migrate_from_json(self) -> None:
        """One-time: convert posts.json to category-subdirectory .md files."""
        json_path = os.path.join(self.data_dir, "posts.json")
        if not os.path.exists(json_path):
            return
        # Only migrate if the posts dir is completely empty
        if any(True for _ in os.walk(self.posts_dir)
               for f in _[2] if f.endswith(".md")):
            return
        try:
            with open(json_path, "r") as f:
                posts = json.load(f)
        except (json.JSONDecodeError, FileNotFoundError):
            return
        for post_dict in posts:
            try:
                cats = post_dict.get("categories") or []
                category = cats[0] if cats else ""
                post = BlogPost(
                    id=post_dict["id"],
                    title=post_dict["title"],
                    slug=post_dict["slug"],
                    markdown_body=post_dict.get("markdown_body", ""),
                    category=category,
                    published=post_dict["published"],
                    created_at=datetime.fromisoformat(post_dict["created_at"]),
                    updated_at=datetime.fromisoformat(post_dict["updated_at"]),
                )
                path = self._unique_path(post.category, post.slug)
                self._write_post(post, path)
            except (KeyError, ValueError):
                continue
        os.rename(json_path, json_path + ".migrated")

    def _migrate_flat_md_files(self) -> None:
        """Move any .md files sitting directly in posts/ into category subdirs."""
        for fname in list(os.listdir(self.posts_dir)):
            if not fname.endswith(".md"):
                continue
            old_path = os.path.join(self.posts_dir, fname)
            if not os.path.isfile(old_path):
                continue
            try:
                meta, body = _parse_md_file(old_path)
                # Resolve category from frontmatter (support old 'categories' list)
                category = meta.get("category")
                if category is None:
                    cats = meta.get("categories") or []
                    category = cats[0] if cats else ""
                    meta["category"] = category
                    meta.pop("categories", None)
                slug = meta.get("slug") or fname[:-3]
                new_path = self._unique_path(category, slug)
                _write_md_file(new_path, meta, body)
                os.remove(old_path)
            except Exception:
                continue

    def _migrate_slug_mismatches(self) -> None:
        """Repair posts whose frontmatter slug does not match their filename.

        This arose from a bug in create_post() where _unique_path() could
        assign a filename like my-post-2.md while the frontmatter still
        recorded slug: my-post.  The mismatch made get_post_by_slug() resolve
        the URL /post/my-post to the WRONG (often unpublished) file.
        """
        for dirpath, _, filenames in os.walk(self.posts_dir):
            for fname in filenames:
                if not fname.endswith(".md"):
                    continue
                path = os.path.join(dirpath, fname)
                file_slug = fname[:-3]
                try:
                    meta, body = _parse_md_file(path)
                    if meta.get("slug") != file_slug:
                        meta["slug"] = file_slug
                        _write_md_file(path, meta, body)
                except Exception:
                    continue

    # -------------------------------------------------------------------------
    # Public API
    # -------------------------------------------------------------------------

    def create_post(
        self, title: str, markdown_body: str, category: str, published: bool = False
    ) -> BlogPost:
        post = BlogPost.create(title, markdown_body, category, published)
        path = self._unique_path(post.category, post.slug)
        # Sync the slug to the actual filename.  _unique_path() may have added
        # a -2, -3 … suffix to avoid clobbering an existing file.  If we do not
        # update post.slug here, the frontmatter and the filename disagree:
        # the slug URL /post/my-post will always resolve to the FIRST file even
        # when a later file is the one that was published.
        filename_slug = os.path.splitext(os.path.basename(path))[0]
        if filename_slug != post.slug:
            post.slug = filename_slug
        self._write_post(post, path)
        return post

    def get_post(self, post_id: str) -> Optional[BlogPost]:
        path = self._find_path_by_id(post_id)
        return self._read_post(path) if path else None

    def list_posts(
        self, published_only: bool = False, category: str = None
    ) -> List[BlogPost]:
        result = []
        if category:
            # Narrow the walk to the matching category directory only
            cat_dir = os.path.join(self.posts_dir, _category_dirname(category))
            walk_it = os.walk(cat_dir) if os.path.isdir(cat_dir) else []
        else:
            walk_it = os.walk(self.posts_dir)
        for dirpath, _, filenames in walk_it:
            for fname in filenames:
                if not fname.endswith(".md"):
                    continue
                post = self._read_post(os.path.join(dirpath, fname))
                if post is None:
                    continue
                if published_only and not post.published:
                    continue
                result.append(post)
        return sorted(result, key=lambda p: p.created_at, reverse=True)

    def list_categories(self, published_only: bool = False) -> List[dict]:
        """Return [{name, count}] sorted by count desc, then name asc."""
        posts = self.list_posts(published_only=published_only)
        counts: dict = {}
        for post in posts:
            cat = post.category or UNCATEGORIZED
            counts[cat] = counts.get(cat, 0) + 1
        return sorted(
            [{"name": k, "count": v} for k, v in counts.items()],
            key=lambda x: (-x["count"], x["name"]),
        )

    def update_post(
        self,
        post_id: str,
        title: str = None,
        markdown_body: str = None,
        category: str = None,
        published: bool = None,
    ) -> Optional[BlogPost]:
        current_path = self._find_path_by_id(post_id)
        if current_path is None:
            return None
        post = self._read_post(current_path)
        if post is None:
            return None
        old_slug = post.slug
        old_category = post.category
        post.update(title, markdown_body, category, published)
        if post.slug != old_slug or _category_dirname(post.category) != _category_dirname(old_category):
            new_path = self._unique_path(post.category, post.slug)
            self._write_post(post, new_path)
            os.remove(current_path)
        else:
            self._write_post(post, current_path)
        return post

    def append_body(self, post_id: str, data: str) -> Optional[BlogPost]:
        """Append `data` to a post's markdown_body and persist.

        Used by the chunked-upload path: the Apple II client streams long
        post bodies in small pieces (the FujiNet IWM write buffer caps around
        1 KB per request).  The title is untouched, so the slug/filename never
        changes here.
        """
        path = self._find_path_by_id(post_id)
        if path is None:
            return None
        post = self._read_post(path)
        if post is None:
            return None
        post.update(markdown_body=(post.markdown_body + data))
        self._write_post(post, path)
        return post

    def delete_post(self, post_id: str) -> bool:
        path = self._find_path_by_id(post_id)
        if path is None:
            return False
        os.remove(path)
        return True

    def get_stats(self) -> dict:
        """Compute blog statistics over all posts (published + draft).

        Returns a dict matching StatsResponse:
          total_posts, total_categories, avg_bytes, year,
          categories ([{name, count}] sorted by count desc),
          posts_per_month (12 ints, Jan–Dec of the current calendar year).
        """
        from datetime import date

        posts = self.list_posts()
        total_posts = len(posts)

        # Category counts
        cat_counts: dict = {}
        for post in posts:
            cat = post.category or UNCATEGORIZED
            cat_counts[cat] = cat_counts.get(cat, 0) + 1
        total_categories = len(cat_counts)
        categories = sorted(
            [{"name": k, "count": v} for k, v in cat_counts.items()],
            key=lambda x: (-x["count"], x["name"]),
        )

        # Posts per month for the current calendar year
        current_year = date.today().year
        posts_per_month = [0] * 12
        for post in posts:
            if post.created_at.year == current_year:
                posts_per_month[post.created_at.month - 1] += 1

        # Average .md file size (bytes)
        total_size = 0
        file_count = 0
        for dirpath, _, filenames in os.walk(self.posts_dir):
            for fname in filenames:
                if not fname.endswith(".md"):
                    continue
                try:
                    total_size += os.path.getsize(os.path.join(dirpath, fname))
                    file_count += 1
                except OSError:
                    continue
        avg_bytes = int(round(total_size / file_count)) if file_count > 0 else 0

        return {
            "total_posts": total_posts,
            "total_categories": total_categories,
            "avg_bytes": avg_bytes,
            "year": current_year,
            "categories": categories,
            "posts_per_month": posts_per_month,
        }

    def get_post_by_slug(self, slug: str) -> Optional[BlogPost]:
        """Find a post by slug.

        Fast path: the slug usually equals the filename stem (e.g. my-post →
        my-post.md).  Slow path: scan frontmatter for posts that existed before
        the slug-sync fix and whose filename stem differs from their slug field.
        """
        target = f"{slug}.md"
        slow_candidates: list = []
        for dirpath, _, filenames in os.walk(self.posts_dir):
            if target in filenames:
                post = self._read_post(os.path.join(dirpath, target))
                if post is not None:
                    return post
            # Collect mismatched files for the slow path
            for fname in filenames:
                if not fname.endswith(".md") or fname == target:
                    continue
                slow_candidates.append(os.path.join(dirpath, fname))

        for path in slow_candidates:
            try:
                meta, _ = _parse_md_file(path)
                if meta.get("slug") == slug:
                    return self._read_post(path)
            except Exception:
                continue
        return None
