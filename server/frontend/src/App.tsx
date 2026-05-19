import React, { useState, useEffect } from 'react'
import { blogAPI, BlogPost } from './api'
import { PostList } from './components/PostList'
import { PostEditor } from './components/PostEditor'
import './App.css'

type ViewMode = 'list' | 'edit' | 'new' | 'published'

function App() {
  const [viewMode, setViewMode] = useState<ViewMode>('list')
  const [posts, setPosts] = useState<BlogPost[]>([])
  const [loading, setLoading] = useState(false)
  const [error, setError] = useState<string | null>(null)
  const [editingPost, setEditingPost] = useState<BlogPost | null>(null)
  const [newPostData, setNewPostData] = useState({
    title: '',
    categories: [] as string[],
    content: '',
  })

  // Load posts on mount
  useEffect(() => {
    loadPosts()
  }, [])

  const loadPosts = async () => {
    setLoading(true)
    setError(null)
    try {
      const data = await blogAPI.listPosts()
      setPosts(data)
    } catch (err) {
      setError('Failed to load posts')
      console.error(err)
    } finally {
      setLoading(false)
    }
  }

  const handleEdit = (post: BlogPost) => {
    setEditingPost(post)
    setNewPostData({
      title: post.title,
      categories: post.categories,
      content: post.markdown_body,
    })
    setViewMode('edit')
  }

  const handleSaveEdit = async () => {
    if (!editingPost) return
    setLoading(true)
    try {
      const updated = await blogAPI.updatePost(editingPost.id, {
        title: newPostData.title,
        markdown_body: newPostData.content,
        categories: newPostData.categories,
      })
      setPosts(posts.map((p) => (p.id === updated.id ? updated : p)))
      setViewMode('list')
      setEditingPost(null)
      setNewPostData({ title: '', categories: [], content: '' })
    } catch (err) {
      setError('Failed to save post')
      console.error(err)
    } finally {
      setLoading(false)
    }
  }

  const handleSaveNew = async () => {
    setLoading(true)
    try {
      const post = await blogAPI.createPost({
        title: newPostData.title,
        markdown_body: newPostData.content,
        categories: newPostData.categories,
        published: false,
      })
      setPosts([post, ...posts])
      setViewMode('list')
      setNewPostData({ title: '', categories: [], content: '' })
    } catch (err) {
      setError('Failed to create post')
      console.error(err)
    } finally {
      setLoading(false)
    }
  }

  const handleDelete = async (id: string) => {
    if (!window.confirm('Are you sure you want to delete this post?')) return
    setLoading(true)
    try {
      await blogAPI.deletePost(id)
      setPosts(posts.filter((p) => p.id !== id))
    } catch (err) {
      setError('Failed to delete post')
      console.error(err)
    } finally {
      setLoading(false)
    }
  }

  const handleTogglePublish = async (post: BlogPost) => {
    setLoading(true)
    try {
      const updated = await blogAPI.togglePublish(post.id, !post.published)
      setPosts(posts.map((p) => (p.id === updated.id ? updated : p)))
    } catch (err) {
      setError('Failed to toggle publish state')
      console.error(err)
    } finally {
      setLoading(false)
    }
  }

  return (
    <div className="app">
      <header className="app-header">
        <h1>FujiNet Blog</h1>
        <nav className="app-nav">
          <button
            className={`nav-btn ${viewMode === 'list' ? 'active' : ''}`}
            onClick={() => {
              setViewMode('list')
              loadPosts()
            }}
          >
            All Posts
          </button>
          <button
            className={`nav-btn ${viewMode === 'published' ? 'active' : ''}`}
            onClick={() => setViewMode('published')}
          >
            Published
          </button>
          <button
            className={`nav-btn ${viewMode === 'new' ? 'active' : ''}`}
            onClick={() => {
              setViewMode('new')
              setNewPostData({ title: '', categories: [], content: '' })
            }}
          >
            New Post
          </button>
        </nav>
      </header>

      {error && <div className="error-banner">{error}</div>}

      <main className="app-main">
        {viewMode === 'list' && (
          <>
            {loading ? (
              <div className="loading">Loading posts...</div>
            ) : (
              <PostList
                posts={posts}
                onEdit={handleEdit}
                onDelete={handleDelete}
                onTogglePublish={handleTogglePublish}
              />
            )}
          </>
        )}

        {viewMode === 'published' && (
          <>
            {loading ? (
              <div className="loading">Loading posts...</div>
            ) : (
              <PostList
                posts={posts.filter((p) => p.published)}
                onEdit={() => {}}
                onDelete={() => {}}
                onTogglePublish={() => {}}
              />
            )}
          </>
        )}

        {viewMode === 'new' && (
          <PostEditor
            title={newPostData.title}
            categories={newPostData.categories}
            content={newPostData.content}
            onTitleChange={(title) =>
              setNewPostData({ ...newPostData, title })
            }
            onCategoriesChange={(categories) =>
              setNewPostData({ ...newPostData, categories })
            }
            onContentChange={(content) =>
              setNewPostData({ ...newPostData, content })
            }
            onSave={handleSaveNew}
            onCancel={() => setViewMode('list')}
            isSaving={loading}
          />
        )}

        {viewMode === 'edit' && editingPost && (
          <PostEditor
            title={newPostData.title}
            categories={newPostData.categories}
            content={newPostData.content}
            onTitleChange={(title) =>
              setNewPostData({ ...newPostData, title })
            }
            onCategoriesChange={(categories) =>
              setNewPostData({ ...newPostData, categories })
            }
            onContentChange={(content) =>
              setNewPostData({ ...newPostData, content })
            }
            onSave={handleSaveEdit}
            onCancel={() => {
              setViewMode('list')
              setEditingPost(null)
            }}
            isSaving={loading}
          />
        )}
      </main>
    </div>
  )
}

export default App
