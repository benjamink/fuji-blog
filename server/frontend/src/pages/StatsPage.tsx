import { useEffect, useState } from 'react'
import { blogAPI, BlogStats } from '../api'
import { BlogLayout } from './BlogLayout'
import './StatsPage.css'

const MONTHS = ['Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun',
                'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec']

function formatBytes(n: number): string {
  if (n < 1024) return `${n} B`
  if (n < 1024 * 1024) return `${(n / 1024).toFixed(1)} KB`
  return `${(n / (1024 * 1024)).toFixed(1)} MB`
}

export function StatsPage() {
  const [stats, setStats] = useState<BlogStats | null>(null)
  const [loading, setLoading] = useState(true)
  const [error, setError] = useState<string | null>(null)

  useEffect(() => {
    blogAPI
      .getStats()
      .then(setStats)
      .catch(() => setError('Failed to load statistics'))
      .finally(() => setLoading(false))
  }, [])

  if (loading) return <BlogLayout><p className="blog-empty">Loading…</p></BlogLayout>
  if (error)   return <BlogLayout><p className="blog-empty">{error}</p></BlogLayout>
  if (!stats)  return null

  const catMax = Math.max(...stats.categories.map(c => c.count), 1)
  const ppmMax = Math.max(...stats.posts_per_month, 1)

  return (
    <BlogLayout>
      <h1 className="stats-heading">Blog Statistics</h1>

      {/* ── Summary cards ─────────────────────────────────── */}
      <div className="stats-cards">
        <div className="stats-card">
          <span className="stats-card-value">{stats.total_posts}</span>
          <span className="stats-card-label">Total Posts</span>
        </div>
        <div className="stats-card">
          <span className="stats-card-value">{stats.total_categories}</span>
          <span className="stats-card-label">Categories</span>
        </div>
        <div className="stats-card">
          <span className="stats-card-value">{formatBytes(stats.avg_bytes)}</span>
          <span className="stats-card-label">Avg Post Size</span>
        </div>
      </div>

      {/* ── Category breakdown ────────────────────────────── */}
      <section className="stats-section">
        <h2 className="stats-section-title">Posts by Category</h2>
        {stats.categories.length === 0 ? (
          <p className="stats-empty">No posts yet.</p>
        ) : (
          <div className="stats-bars">
            {stats.categories.map(cat => (
              <div key={cat.name} className="stats-bar-row">
                <span className="stats-bar-label stats-bar-label--cat" title={cat.name}>
                  {cat.name}
                </span>
                <div className="stats-bar-track">
                  <div
                    className="stats-bar-fill"
                    style={{ width: `${(cat.count / catMax) * 100}%` }}
                  />
                </div>
                <span className="stats-bar-count">{cat.count}</span>
              </div>
            ))}
          </div>
        )}
      </section>

      {/* ── Monthly histogram ─────────────────────────────── */}
      <section className="stats-section">
        <h2 className="stats-section-title">Posts per Month — {stats.year}</h2>
        <div className="stats-bars">
          {MONTHS.map((month, i) => (
            <div key={month} className="stats-bar-row">
              <span className="stats-bar-label stats-bar-label--month">{month}</span>
              <div className="stats-bar-track">
                <div
                  className="stats-bar-fill stats-bar-fill--month"
                  style={{
                    width: stats.posts_per_month[i] === 0
                      ? '0%'
                      : `${(stats.posts_per_month[i] / ppmMax) * 100}%`,
                  }}
                />
              </div>
              <span className="stats-bar-count">{stats.posts_per_month[i]}</span>
            </div>
          ))}
        </div>
      </section>
    </BlogLayout>
  )
}
