import React from 'react'
import { CategoryInfo } from '../api'
import './CategorySidebar.css'

interface CategorySidebarProps {
  categories: CategoryInfo[]
  selectedCategory: string | null
  onSelect: (category: string | null) => void
}

export const CategorySidebar: React.FC<CategorySidebarProps> = ({
  categories,
  selectedCategory,
  onSelect,
}) => {
  const total = categories.reduce((sum, c) => sum + c.count, 0)

  return (
    <aside className="category-sidebar">
      <h3 className="sidebar-heading">Categories</h3>
      <ul className="sidebar-list">
        <li
          className={`sidebar-item${selectedCategory === null ? ' active' : ''}`}
          onClick={() => onSelect(null)}
        >
          <span className="sidebar-name">All Posts</span>
          <span className="sidebar-count">{total}</span>
        </li>
        {categories.map((cat) => (
          <li
            key={cat.name}
            className={`sidebar-item${selectedCategory === cat.name ? ' active' : ''}`}
            onClick={() => onSelect(cat.name)}
          >
            <span className="sidebar-name">{cat.name}</span>
            <span className="sidebar-count">{cat.count}</span>
          </li>
        ))}
      </ul>
    </aside>
  )
}
