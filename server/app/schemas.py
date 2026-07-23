from pydantic import BaseModel, Field
from typing import Optional
from datetime import datetime


class LoginRequest(BaseModel):
    """Request body for admin login."""
    username: str
    password: str


class TokenResponse(BaseModel):
    """JWT token returned after successful login."""
    access_token: str
    token_type: str = "bearer"


class ApiKeyResponse(BaseModel):
    """The Apple II client's pre-shared API key and where it comes from."""
    api_key: str
    source: str  # "file" (generated), "env" (API_KEY var), or "none"


class ApiKeyImportRequest(BaseModel):
    """Request body for importing a client API key from a scanned QR code."""
    api_key: str


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
    Keeps the response small enough for FujiNet's receive buffer.

    published is typed as int (1/0) rather than bool (true/false) so that
    network_json_query on the FujiNet IWM firmware can read it: the firmware's
    JSON query primitive only handles string/number leaf values and returns an
    empty string for JSON boolean primitives."""
    id: str
    title: str
    slug: str
    category: str
    published: int
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


class CategoryCount(BaseModel):
    """Category name with its post count."""
    name: str
    count: int


class StatsResponse(BaseModel):
    """Blog statistics response."""
    total_posts: int
    total_categories: int
    # average post file size, rounded to nearest byte
    avg_bytes: int
    # calendar year the posts_per_month refers to
    year: int
    # sorted by count desc, then name asc
    categories: list[CategoryCount]
    # 12 entries: Jan–Dec counts for `year`
    posts_per_month: list[int]
