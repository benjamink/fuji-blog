import React from 'react'
import { BlogPost } from '../api'
import './PostList.css'

interface PostListProps {
  posts: BlogPost[]
  onEdit: (post: BlogPost) => void
  onDelete: (id: string) => void
  onTogglePublish: (post: BlogPost) => void
  onCategoryClick?: (category: string) => void
  showActions?: boolean
}

export const PostList: React.FC<PostListProps> = ({
  posts,
  onEdit,
  onDelete,
  onTogglePublish,
  onCategoryClick,
  showActions = true,
}) => {
  if (posts.length === 0) {
    return <div className="post-list-empty">No posts yet. Create one to get started!</div>
  }

  return (
    <div className="post-list">
      {posts.map((post) => (
        <div key={post.id} className="post-card">
          <div className="post-header">
            <h2 className="post-title">{post.title}</h2>
            {showActions && (
              <span className={`post-status ${post.published ? 'published' : 'draft'}`}>
                {post.published ? 'Published' : 'Draft'}
              </span>
            )}
          </div>

          <div className="post-meta">
            <span className="post-date">
              {new Date(post.created_at).toLocaleDateString()}
            </span>
            {post.category && (
              <div className="post-categories">
                <span
                  className={`category-badge${onCategoryClick ? ' clickable' : ''}`}
                  onClick={() => onCategoryClick?.(post.category)}
                  title={onCategoryClick ? `Filter by "${post.category}"` : undefined}
                >
                  {post.category}
                </span>
              </div>
            )}
          </div>

          <div className="post-preview">
            {post.markdown_body?.substring(0, 150)}
            {(post.markdown_body?.length ?? 0) > 150 ? '...' : ''}
          </div>

          {showActions && (
            <div className="post-actions">
              <button onClick={() => onEdit(post)} className="btn btn-primary">
                Edit
              </button>
              <button onClick={() => onTogglePublish(post)} className="btn btn-secondary">
                {post.published ? 'Unpublish' : 'Publish'}
              </button>
              <button onClick={() => onDelete(post.id)} className="btn btn-danger">
                Delete
              </button>
            </div>
          )}
        </div>
      ))}
    </div>
  )
}
