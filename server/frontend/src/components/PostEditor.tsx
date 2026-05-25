import React from 'react'
import './PostEditor.css'

interface PostEditorProps {
  title: string
  category: string
  content: string
  onTitleChange: (title: string) => void
  onCategoryChange: (category: string) => void
  onContentChange: (content: string) => void
  onSave: () => void
  onCancel: () => void
  isSaving?: boolean
}

export const PostEditor: React.FC<PostEditorProps> = ({
  title,
  category,
  content,
  onTitleChange,
  onCategoryChange,
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
          placeholder="Category (e.g. tech, apple2)"
          value={category}
          onChange={(e) => onCategoryChange(e.target.value)}
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
