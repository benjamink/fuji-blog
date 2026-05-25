import React, { useEffect, useState } from 'react'
import { blogAPI, BlogPost } from '../api'
import { BlogLayout } from './BlogLayout'
import { BlogPostCard } from './BlogPostCard'

export function BlogHome() {
  const [posts, setPosts] = useState<BlogPost[]>([])
  const [loading, setLoading] = useState(true)
  const [error, setError] = useState<string | null>(null)

  useEffect(() => {
    blogAPI
      .listPublishedPosts()
      .then(setPosts)
      .catch(() => setError('Failed to load posts'))
      .finally(() => setLoading(false))
  }, [])

  return (
    <BlogLayout>
      {loading && <p className="blog-empty">Loading…</p>}
      {error && <p className="blog-empty">{error}</p>}
      {!loading && !error && (
        <>
          <p className="blog-page-heading">All Posts</p>
          {posts.length === 0 ? (
            <p className="blog-empty">No posts published yet.</p>
          ) : (
            <div className="blog-posts">
              {posts.map((post) => (
                <BlogPostCard key={post.id} post={post} />
              ))}
            </div>
          )}
        </>
      )}
    </BlogLayout>
  )
}
