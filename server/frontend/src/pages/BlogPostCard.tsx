import React from 'react'
import { Link } from 'react-router-dom'
import { BlogPost } from '../api'
import { getExcerpt, formatDate } from './blogUtils'

interface BlogPostCardProps {
  post: BlogPost
}

export function BlogPostCard({ post }: BlogPostCardProps) {
  return (
    <article className="blog-card">
      <h2 className="blog-card-title">
        <Link to={`/post/${post.slug}`}>{post.title}</Link>
      </h2>

      <div className="blog-card-meta">
        <time dateTime={post.created_at}>{formatDate(post.created_at)}</time>
        {post.category && (
          <Link
            to={`/${encodeURIComponent(post.category)}`}
            className="blog-card-cat"
          >
            {post.category}
          </Link>
        )}
      </div>

      <p className="blog-card-excerpt">{getExcerpt(post.markdown_body)}</p>
    </article>
  )
}
