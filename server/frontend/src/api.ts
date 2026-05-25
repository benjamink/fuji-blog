import axios from 'axios'

const API_BASE = '/api'

export interface BlogPost {
  id: string
  title: string
  slug: string
  markdown_body: string
  html_body?: string
  category: string
  published: boolean
  created_at: string
  updated_at: string
}

export interface CategoryInfo {
  name: string
  count: number
}

export interface BlogStats {
  total_posts: number
  total_categories: number
  avg_bytes: number          // average post file size in bytes
  year: number               // calendar year for posts_per_month
  categories: CategoryInfo[] // sorted by count desc
  posts_per_month: number[]  // 12 entries, Jan–Dec
}

export interface CreatePostRequest {
  title: string
  markdown_body: string
  category: string
  published?: boolean
}

export interface UpdatePostRequest {
  title?: string
  markdown_body?: string
  category?: string
  published?: boolean
}

const api = axios.create({
  baseURL: API_BASE,
})

export const blogAPI = {
  // List all posts (admin view)
  async listPosts(category?: string): Promise<BlogPost[]> {
    const { data } = await api.get('/posts', { params: category ? { category } : {} })
    return data
  },

  // List published posts only
  async listPublishedPosts(category?: string): Promise<BlogPost[]> {
    const { data } = await api.get('/posts/published', {
      params: category ? { category } : {},
    })
    return data
  },

  // List all categories
  async listCategories(publishedOnly = false): Promise<CategoryInfo[]> {
    const { data } = await api.get('/categories', {
      params: publishedOnly ? { published_only: true } : {},
    })
    return data
  },

  // Get a single post by ID
  async getPost(id: string): Promise<BlogPost> {
    const { data } = await api.get(`/posts/${id}`)
    return data
  },

  // Get a published post by URL slug (used by the public blog)
  async getPostBySlug(slug: string): Promise<BlogPost> {
    const { data } = await api.get(`/posts/slug/${slug}`)
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

  // Blog statistics
  async getStats(): Promise<BlogStats> {
    const { data } = await api.get('/stats')
    return data
  },
}
