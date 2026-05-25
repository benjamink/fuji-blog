import React, { useEffect, useState } from 'react'
import { Link, useLocation } from 'react-router-dom'
import { blogAPI, CategoryInfo } from '../api'
import './blog.css'

interface BlogLayoutProps {
  children: React.ReactNode
}

export function BlogLayout({ children }: BlogLayoutProps) {
  const [categories, setCategories] = useState<CategoryInfo[]>([])
  const location = useLocation()

  useEffect(() => {
    blogAPI.listCategories(true).then(setCategories).catch(console.error)
  }, [])

  // Derive the active category from the current path (e.g. "/tech" → "tech").
  // Reserved literal paths (/stats, /admin, /post/…) are not categories.
  const reservedPaths = new Set(['/stats', '/admin'])
  const activeCat =
    location.pathname === '/'
      ? null
      : location.pathname.startsWith('/post/') || reservedPaths.has(location.pathname)
      ? null
      : decodeURIComponent(location.pathname.slice(1))

  return (
    <div className="blog-layout">
      <header className="blog-header">
        <div className="blog-header-inner">
          <Link to="/" className="blog-site-title">
            FujiBlogger
          </Link>

          <nav className="blog-cat-nav">
            <Link
              to="/"
              className={`blog-cat-link${activeCat === null && location.pathname === '/' ? ' active' : ''}`}
            >
              All Posts
            </Link>
            {categories.map((cat) => (
              <Link
                key={cat.name}
                to={`/${encodeURIComponent(cat.name)}`}
                className={`blog-cat-link${activeCat === cat.name ? ' active' : ''}`}
              >
                {cat.name}
              </Link>
            ))}
          </nav>

          <div className="blog-header-actions">
            <Link
              to="/stats"
              className={`blog-cat-link${location.pathname === '/stats' ? ' active' : ''}`}
            >
              Stats
            </Link>
            <a href="/admin" className="blog-admin-link">Admin</a>
          </div>
        </div>
      </header>

      <main className="blog-main">{children}</main>
    </div>
  )
}
