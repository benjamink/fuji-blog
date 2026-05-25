from pydantic import BaseModel, Field
from typing import Optional
from datetime import datetime


class BlogPostCreate(BaseModel):
    """Request model for creating a new blog post."""
    title: str = Field(..., min_length=1, max_length=200)
    markdown_body: str = Field(..., min_length=1)
    category: str = Field(default="")
    published: bool = Field(default=False)


class BlogPostUpdate(BaseModel):
    """Request model for updating a blog post."""
    title: Optional[str] = Field(None, min_length=1, max_length=200)
    markdown_body: Optional[str] = Field(None, min_length=1)
    category: Optional[str] = None
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
    category: str
    published: bool
    created_at: datetime
    updated_at: datetime


class BlogPostSummary(BaseModel):
    """Compact response for write operations (no body fields).
    Keeps the response small enough for FujiNet's receive buffer."""
    id: str
    title: str
    slug: str
    category: str
    published: bool
    created_at: datetime
    updated_at: datetime


class BlogPostMarkdown(BaseModel):
    """Slim read response: title + category + raw markdown only.
    No html_body, keeping the payload small enough for network_json_parse."""
    title: str
    category: str
    markdown_body: str


class RenderRequest(BaseModel):
    """Request model for Markdown rendering."""
    markdown_body: str = Field(..., min_length=1)


class RenderResponse(BaseModel):
    """Response model for rendered HTML."""
    html: str
