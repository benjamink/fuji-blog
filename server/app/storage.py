import json
import os
from typing import List, Optional
from datetime import datetime
from .models import BlogPost


class BlogStorage:
    """File-based persistence layer for blog posts."""

    def __init__(self, data_dir: str = None):
        """Initialize storage with a data directory path."""
        if data_dir is None:
            data_dir = os.path.join(os.path.dirname(__file__), "..", "data")
        self.data_dir = data_dir
        self.posts_file = os.path.join(data_dir, "posts.json")
        os.makedirs(data_dir, exist_ok=True)
        self._ensure_posts_file()

    def _ensure_posts_file(self) -> None:
        """Ensure the posts.json file exists."""
        if not os.path.exists(self.posts_file):
            with open(self.posts_file, "w") as f:
                json.dump([], f)

    def _load_posts(self) -> List[dict]:
        """Load all posts from JSON file."""
        try:
            with open(self.posts_file, "r") as f:
                data = json.load(f)
                return data if isinstance(data, list) else []
        except (json.JSONDecodeError, FileNotFoundError):
            return []

    def _save_posts(self, posts: List[dict]) -> None:
        """Save all posts to JSON file."""
        with open(self.posts_file, "w") as f:
            json.dump(posts, f, indent=2, default=str)

    def _dict_to_post(self, post_dict: dict) -> BlogPost:
        """Convert a dictionary to a BlogPost object."""
        return BlogPost(
            id=post_dict["id"],
            title=post_dict["title"],
            slug=post_dict["slug"],
            markdown_body=post_dict["markdown_body"],
            categories=post_dict.get("categories", []),
            published=post_dict["published"],
            created_at=datetime.fromisoformat(post_dict["created_at"]),
            updated_at=datetime.fromisoformat(post_dict["updated_at"]),
            html_body=post_dict.get("html_body", ""),
        )

    def create_post(
        self, title: str, markdown_body: str, categories: List[str], published: bool = False
    ) -> BlogPost:
        """Create and persist a new blog post."""
        post = BlogPost.create(title, markdown_body, categories, published)
        posts = self._load_posts()
        posts.append(post.to_dict())
        self._save_posts(posts)
        return post

    def get_post(self, post_id: str) -> Optional[BlogPost]:
        """Retrieve a post by ID."""
        posts = self._load_posts()
        for post_dict in posts:
            if post_dict["id"] == post_id:
                return self._dict_to_post(post_dict)
        return None

    def list_posts(self, published_only: bool = False) -> List[BlogPost]:
        """List all posts, optionally filtering to published only."""
        posts = self._load_posts()
        result = []
        for post_dict in posts:
            if published_only and not post_dict["published"]:
                continue
            result.append(self._dict_to_post(post_dict))
        return sorted(result, key=lambda p: p.created_at, reverse=True)

    def update_post(
        self,
        post_id: str,
        title: str = None,
        markdown_body: str = None,
        categories: List[str] = None,
        published: bool = None,
    ) -> Optional[BlogPost]:
        """Update a post by ID."""
        posts = self._load_posts()
        for post_dict in posts:
            if post_dict["id"] == post_id:
                post = self._dict_to_post(post_dict)
                post.update(title, markdown_body, categories, published)
                post_dict.update(post.to_dict())
                self._save_posts(posts)
                return post
        return None

    def delete_post(self, post_id: str) -> bool:
        """Delete a post by ID. Returns True if deleted, False if not found."""
        posts = self._load_posts()
        for i, post_dict in enumerate(posts):
            if post_dict["id"] == post_id:
                posts.pop(i)
                self._save_posts(posts)
                return True
        return False
