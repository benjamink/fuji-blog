import React, { useState, useEffect } from 'react'
import { blogAPI, BlogPost } from './api'
import { PostList } from './components/PostList'
import { PostEditor } from './components/PostEditor'
import './App.css'

type ViewMode = 'list' | 'edit' | 'new'

function AdminApp() {
  const [viewMode, setViewMode] = useState<ViewMode>('list')
  const [posts, setPosts] = useState<BlogPost[]>([])
  const [loading, setLoading] = useState(false)
  const [error, setError] = useState<string | null>(null)
  const [editingPost, setEditingPost] = useState<BlogPost | null>(null)
  const [formData, setFormData] = useState({ title: '', category: '', content: '' })
  const [adminCategory, setAdminCategory] = useState<string>('')

  useEffect(() => {
    loadPosts()
  }, [])

  const loadPosts = async () => {
    setLoading(true)
    setError(null)
    try {
      setPosts(await blogAPI.listPosts())
    } catch {
      setError('Failed to load posts')
    } finally {
      setLoading(false)
    }
  }

  const handleEdit = (post: BlogPost) => {
    setEditingPost(post)
    setFormData({ title: post.title, category: post.category, content: post.markdown_body })
    setViewMode('edit')
  }

  const handleSaveEdit = async () => {
    if (!editingPost) return
    setLoading(true)
    try {
      const updated = await blogAPI.updatePost(editingPost.id, {
        title: formData.title,
        markdown_body: formData.content,
        category: formData.category,
      })
      setPosts(posts.map((p) => (p.id === updated.id ? updated : p)))
      setViewMode('list')
      setEditingPost(null)
      setFormData({ title: '', category: '', content: '' })
    } catch {
      setError('Failed to save post')
    } finally {
      setLoading(false)
    }
  }

  const handleSaveNew = async () => {
    setLoading(true)
    try {
      const post = await blogAPI.createPost({
        title: formData.title,
        markdown_body: formData.content,
        category: formData.category,
        published: false,
      })
      setPosts([post, ...posts])
      setViewMode('list')
      setFormData({ title: '', category: '', content: '' })
    } catch {
      setError('Failed to create post')
    } finally {
      setLoading(false)
    }
  }

  const handleDelete = async (id: string) => {
    if (!window.confirm('Delete this post?')) return
    setLoading(true)
    try {
      await blogAPI.deletePost(id)
      setPosts(posts.filter((p) => p.id !== id))
    } catch {
      setError('Failed to delete post')
    } finally {
      setLoading(false)
    }
  }

  const handleTogglePublish = async (post: BlogPost) => {
    setLoading(true)
    try {
      const updated = await blogAPI.togglePublish(post.id, !post.published)
      setPosts(posts.map((p) => (p.id === updated.id ? updated : p)))
    } catch {
      setError('Failed to toggle publish state')
    } finally {
      setLoading(false)
    }
  }

  const adminCategories = Array.from(
    new Set(posts.map((p) => p.category).filter(Boolean))
  ).sort()
  const visiblePosts = adminCategory
    ? posts.filter((p) => p.category === adminCategory)
    : posts

  return (
    <div className="app">
      <header className="app-header">
        <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '16px' }}>
          <h1 style={{ margin: 0 }}>FujiBlogger — Admin</h1>
          <a href="/" style={{ color: 'rgba(255,255,255,0.8)', fontSize: '14px', textDecoration: 'none' }}>
            ← View Blog
          </a>
        </div>
        <nav className="app-nav">
          <button
            className={`nav-btn ${viewMode === 'list' ? 'active' : ''}`}
            onClick={() => { setViewMode('list'); setAdminCategory(''); loadPosts() }}
          >
            All Posts
          </button>
          <button
            className={`nav-btn ${viewMode === 'new' ? 'active' : ''}`}
            onClick={() => { setViewMode('new'); setFormData({ title: '', category: '', content: '' }) }}
          >
            New Post
          </button>
        </nav>
      </header>

      {error && <div className="error-banner">{error}</div>}

      <main className="app-main">
        {viewMode === 'list' && (
          <>
            {adminCategories.length > 0 && (
              <div className="admin-filter-bar">
                <label htmlFor="admin-cat-filter">Filter:</label>
                <select
                  id="admin-cat-filter"
                  value={adminCategory}
                  onChange={(e) => setAdminCategory(e.target.value)}
                  className="admin-cat-select"
                >
                  <option value="">All categories</option>
                  {adminCategories.map((cat) => (
                    <option key={cat} value={cat}>{cat}</option>
                  ))}
                </select>
                {adminCategory && (
                  <button className="clear-filter-btn" onClick={() => setAdminCategory('')}>
                    ✕ Clear
                  </button>
                )}
              </div>
            )}
            {loading ? (
              <div className="loading">Loading posts…</div>
            ) : (
              <PostList
                posts={visiblePosts}
                onEdit={handleEdit}
                onDelete={handleDelete}
                onTogglePublish={handleTogglePublish}
                onCategoryClick={(cat) => setAdminCategory(cat)}
              />
            )}
          </>
        )}

        {viewMode === 'new' && (
          <PostEditor
            title={formData.title}
            category={formData.category}
            content={formData.content}
            onTitleChange={(title) => setFormData({ ...formData, title })}
            onCategoryChange={(category) => setFormData({ ...formData, category })}
            onContentChange={(content) => setFormData({ ...formData, content })}
            onSave={handleSaveNew}
            onCancel={() => setViewMode('list')}
            isSaving={loading}
          />
        )}

        {viewMode === 'edit' && editingPost && (
          <PostEditor
            title={formData.title}
            category={formData.category}
            content={formData.content}
            onTitleChange={(title) => setFormData({ ...formData, title })}
            onCategoryChange={(category) => setFormData({ ...formData, category })}
            onContentChange={(content) => setFormData({ ...formData, content })}
            onSave={handleSaveEdit}
            onCancel={() => { setViewMode('list'); setEditingPost(null) }}
            isSaving={loading}
          />
        )}
      </main>
    </div>
  )
}

export default AdminApp
