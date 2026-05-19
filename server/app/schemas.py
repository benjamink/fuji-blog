from pydantic import BaseModel, Field
from typing import List, Optional
from datetime import datetime


class BlogPostCreate(BaseModel):
    """Request model for creating a new blog post."""
    title: str = Field(..., min_length=1, max_length=200)
    markdown_body: str = Field(..., min_length=1)
    categories: List[str] = Field(default_factory=list)
    published: bool = Field(default=False)


class BlogPostUpdate(BaseModel):
    """Request model for updating a blog post."""
    title: Optional[str] = Field(None, min_length=1, max_length=200)
    markdown_body: Optional[str] = Field(None, min_length=1)
    categories: Optional[List[str]] = None
    published: Optional[bool] = None


class BlogPostPublish(BaseModel):
    """Request model for toggling publish state."""
    published: bool


class BlogPostResponse(BaseModel):
    """Response model for a blog post."""
    id: str
    title: str
    slug: str
    markdown_body: str
    html_body: Optional[str] = None
    categories: List[str]
    published: bool
    created_at: datetime
    updated_at: datetime


class RenderRequest(BaseModel):
    """Request model for Markdown rendering."""
    markdown_body: str = Field(..., min_length=1)


class RenderResponse(BaseModel):
    """Response model for rendered HTML."""
    html: str
