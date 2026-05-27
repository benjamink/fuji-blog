/**
 * Authentication helpers for the FujiBlogger admin UI.
 *
 * Tokens are stored in localStorage under AUTH_KEY so they survive page
 * refreshes.  Clearing the token (logout) forces the ProtectedRoute to
 * redirect back to the login page.
 */

const AUTH_KEY = 'fujiblogger_admin_token'

// ---------------------------------------------------------------------------
// Token storage
// ---------------------------------------------------------------------------

export function getToken(): string | null {
  return localStorage.getItem(AUTH_KEY)
}

export function setToken(token: string): void {
  localStorage.setItem(AUTH_KEY, token)
}

export function clearToken(): void {
  localStorage.removeItem(AUTH_KEY)
}

// ---------------------------------------------------------------------------
// Auth API calls
// ---------------------------------------------------------------------------

interface TokenResponse {
  access_token: string
  token_type: string
}

/**
 * POST /api/auth/login — exchange credentials for a JWT.
 * Stores the token on success and throws on failure.
 */
export async function login(username: string, password: string): Promise<void> {
  const res = await fetch('/api/auth/login', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ username, password }),
  })

  if (!res.ok) {
    const data = await res.json().catch(() => ({}))
    throw new Error((data as { detail?: string }).detail ?? 'Login failed')
  }

  const data: TokenResponse = await res.json()
  setToken(data.access_token)
}

/**
 * Clear the stored token and navigate to the login page.
 */
export function logout(): void {
  clearToken()
  window.location.href = '/admin/login'
}
