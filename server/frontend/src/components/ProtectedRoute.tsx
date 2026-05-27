import React from 'react'
import { Navigate } from 'react-router-dom'
import { getToken } from '../auth'

interface ProtectedRouteProps {
  children: React.ReactNode
}

/**
 * Renders children when a JWT token is present in localStorage.
 * Redirects to /admin/login otherwise.
 */
export function ProtectedRoute({ children }: ProtectedRouteProps) {
  if (!getToken()) {
    return <Navigate to="/admin/login" replace />
  }
  return <>{children}</>
}
