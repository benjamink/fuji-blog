import React, { useEffect, useState } from 'react'
import { useParams } from 'react-router-dom'
import { blogAPI, BlogPost } from '../api'
import { BlogLayout } from './BlogLayout'
import { BlogPostCard } from './BlogPostCard'

export function BlogCategory() {
  const { category } = useParams<{ category: string }>()
  const [posts, setPosts] = useState<BlogPost[]>([])
  const [loading, setLoading] = useState(true)
  const [error, setError] = useState<string | null>(null)

  useEffect(() => {
    if (!category) return
    setLoading(true)
    setError(null)
    blogAPI
      .listPublishedPosts(category)
      .then(setPosts)
      .catch(() => setError('Failed to load posts'))
      .finally(() => setLoading(false))
  }, [category])

  return (
    <BlogLayout>
      {loading && <p className="blog-empty">Loading…</p>}
      {error && <p className="blog-empty">{error}</p>}
      {!loading && !error && (
        <>
          <p className="blog-page-heading">{category}</p>
          {posts.length === 0 ? (
            <p className="blog-empty">No posts in this category.</p>
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
