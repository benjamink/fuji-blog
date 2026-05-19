import React, { useState } from 'react'
import './PostEditor.css'

interface PostEditorProps {
  title: string
  categories: string[]
  content: string
  onTitleChange: (title: string) => void
  onCategoriesChange: (categories: string[]) => void
  onContentChange: (content: string) => void
  onSave: () => void
  onCancel: () => void
  isSaving?: boolean
}

export const PostEditor: React.FC<PostEditorProps> = ({
  title,
  categories,
  content,
  onTitleChange,
  onCategoriesChange,
  onContentChange,
  onSave,
  onCancel,
  isSaving = false,
}) => {
  return (
    <div className="post-editor">
      <div className="editor-header">
        <input
          type="text"
          placeholder="Post Title"
          value={title}
          onChange={(e) => onTitleChange(e.target.value)}
          className="title-input"
        />
      </div>

      <div className="editor-fields">
        <input
          type="text"
          placeholder="Categories (comma-separated)"
          value={categories.join(', ')}
          onChange={(e) =>
            onCategoriesChange(
              e.target.value.split(',').map((c) => c.trim()).filter((c) => c)
            )
          }
          className="categories-input"
        />
      </div>

      <textarea
        value={content}
        onChange={(e) => onContentChange(e.target.value)}
        placeholder="Write your markdown here..."
        className="content-textarea"
      />

      <div className="editor-actions">
        <button onClick={onSave} disabled={isSaving} className="save-btn">
          {isSaving ? 'Saving...' : 'Save'}
        </button>
        <button onClick={onCancel} disabled={isSaving} className="cancel-btn">
          Cancel
        </button>
      </div>
    </div>
  )
}
