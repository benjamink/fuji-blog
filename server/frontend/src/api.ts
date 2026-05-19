import axios from 'axios'

const API_BASE = '/api'

export interface BlogPost {
  id: string
  title: string
  slug: string
  markdown_body: string
  html_body?: string
  categories: string[]
  published: boolean
  created_at: string
  updated_at: string
}

export interface CreatePostRequest {
  title: string
  markdown_body: string
  categories: string[]
  published?: boolean
}

export interface UpdatePostRequest {
  title?: string
  markdown_body?: string
  categories?: string[]
  published?: boolean
}

const api = axios.create({
  baseURL: API_BASE,
})

export const blogAPI = {
  // List all posts (admin view)
  async listPosts(): Promise<BlogPost[]> {
    const { data } = await api.get('/posts')
    return data
  },

  // List published posts only
  async listPublishedPosts(): Promise<BlogPost[]> {
    const { data } = await api.get('/posts/published')
    return data
  },

  // Get a single post
  async getPost(id: string): Promise<BlogPost> {
    const { data } = await api.get(`/posts/${id}`)
    return data
  },

  // Create a new post
  async createPost(post: CreatePostRequest): Promise<BlogPost> {
    const { data } = await api.post('/posts', post)
    return data
  },

  // Update a post
  async updatePost(id: string, post: UpdatePostRequest): Promise<BlogPost> {
    const { data } = await api.put(`/posts/${id}`, post)
    return data
  },

  // Toggle publish state
  async togglePublish(id: string, published: boolean): Promise<BlogPost> {
    const { data } = await api.patch(`/posts/${id}/publish`, { published })
    return data
  },

  // Delete a post
  async deletePost(id: string): Promise<void> {
    await api.delete(`/posts/${id}`)
  },

  // Render markdown to HTML
  async renderMarkdown(markdown: string): Promise<string> {
    const { data } = await api.post('/render', { markdown_body: markdown })
    return data.html
  },
}
