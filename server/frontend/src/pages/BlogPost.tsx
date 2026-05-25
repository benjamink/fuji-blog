import React, { useEffect, useState } from 'react'
import { Link, useParams, useNavigate } from 'react-router-dom'
import { blogAPI, BlogPost as BlogPostType } from '../api'
import { BlogLayout } from './BlogLayout'
import { formatDate } from './blogUtils'

export function BlogPost() {
  const { slug } = useParams<{ slug: string }>()
  const navigate = useNavigate()
  const [post, setPost] = useState<BlogPostType | null>(null)
  const [loading, setLoading] = useState(true)

  useEffect(() => {
    if (!slug) return
    blogAPI
      .getPostBySlug(slug)
      .then(setPost)
      .catch(() => navigate('/', { replace: true }))
      .finally(() => setLoading(false))
  }, [slug])

  return (
    <BlogLayout>
      {loading && <p className="blog-empty">Loading…</p>}

      {!loading && post && (
        <article className="blog-post">
          {/* Back links */}
          <div style={{ display: 'flex', gap: '16px', marginBottom: '28px' }}>
            <Link to="/" className="blog-back">
              ← All Posts
            </Link>
            {post.category && (
              <Link
                to={`/${encodeURIComponent(post.category)}`}
                className="blog-back"
              >
                ← {post.category}
              </Link>
            )}
          </div>

          <h1 className="blog-post-title">{post.title}</h1>

          <div className="blog-post-meta">
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

          <div
            className="prose"
            dangerouslySetInnerHTML={{ __html: post.html_body ?? '' }}
          />
        </article>
      )}
    </BlogLayout>
  )
}
