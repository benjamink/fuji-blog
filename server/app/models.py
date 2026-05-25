from dataclasses import dataclass, asdict
from datetime import datetime
from typing import Optional
import uuid
import re


@dataclass
class BlogPost:
    """In-memory representation of a blog post."""
    id: str
    title: str
    slug: str
    markdown_body: str
    category: str
    published: bool
    created_at: datetime
    updated_at: datetime
    html_body: str = ""

    @classmethod
    def create(
        cls,
        title: str,
        markdown_body: str,
        category: str,
        published: bool = False,
    ) -> "BlogPost":
        now = datetime.utcnow()
        return cls(
            id=str(uuid.uuid4()),
            title=title,
            slug=cls.generate_slug(title),
            markdown_body=markdown_body,
            category=category or "",
            published=published,
            created_at=now,
            updated_at=now,
        )

    @staticmethod
    def generate_slug(title: str) -> str:
        slug = title.lower().strip()
        slug = re.sub(r"[^\w\s-]", "", slug)
        slug = re.sub(r"[-\s]+", "-", slug)
        return slug.strip("-")

    def to_dict(self) -> dict:
        return asdict(self)

    def update(
        self,
        title: Optional[str] = None,
        markdown_body: Optional[str] = None,
        category: Optional[str] = None,
        published: Optional[bool] = None,
    ) -> None:
        if title is not None:
            self.title = title
            self.slug = self.generate_slug(title)
        if markdown_body is not None:
            self.markdown_body = markdown_body
        if category is not None:
            self.category = category
        if published is not None:
            self.published = published
        self.updated_at = datetime.utcnow()
