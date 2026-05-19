from dataclasses import dataclass, asdict, field
from datetime import datetime
from typing import List
import uuid
import re


@dataclass
class BlogPost:
    """In-memory representation of a blog post."""
    id: str
    title: str
    slug: str
    markdown_body: str
    categories: List[str]
    published: bool
    created_at: datetime
    updated_at: datetime
    html_body: str = ""

    @classmethod
    def create(
        cls,
        title: str,
        markdown_body: str,
        categories: List[str],
        published: bool = False,
    ) -> "BlogPost":
        """Create a new BlogPost with auto-generated id, slug, and timestamps."""
        now = datetime.utcnow()
        post_id = str(uuid.uuid4())
        slug = cls.generate_slug(title)
        return cls(
            id=post_id,
            title=title,
            slug=slug,
            markdown_body=markdown_body,
            categories=categories or [],
            published=published,
            created_at=now,
            updated_at=now,
        )

    @staticmethod
    def generate_slug(title: str) -> str:
        """Generate a URL-safe slug from a title."""
        slug = title.lower().strip()
        slug = re.sub(r"[^\w\s-]", "", slug)
        slug = re.sub(r"[-\s]+", "-", slug)
        return slug.strip("-")

    def to_dict(self) -> dict:
        """Convert to dictionary for JSON serialization."""
        return asdict(self)

    def update(
        self,
        title: str = None,
        markdown_body: str = None,
        categories: List[str] = None,
        published: bool = None,
    ) -> None:
        """Update post fields and set updated_at timestamp."""
        if title is not None:
            self.title = title
            self.slug = self.generate_slug(title)
        if markdown_body is not None:
            self.markdown_body = markdown_body
        if categories is not None:
            self.categories = categories
        if published is not None:
            self.published = published
        self.updated_at = datetime.utcnow()
