import { Routes, Route } from 'react-router-dom'
import { BlogHome } from './pages/BlogHome'
import { BlogCategory } from './pages/BlogCategory'
import { BlogPost } from './pages/BlogPost'
import { StatsPage } from './pages/StatsPage'
import { LoginPage } from './pages/LoginPage'
import { ProtectedRoute } from './components/ProtectedRoute'
import AdminApp from './AdminApp'

function App() {
  return (
    <Routes>
      {/* Public blog */}
      <Route path="/" element={<BlogHome />} />
      <Route path="/post/:slug" element={<BlogPost />} />

      {/* Stats — literal segment takes priority over /:category */}
      <Route path="/stats" element={<StatsPage />} />

      {/* Admin login */}
      <Route path="/admin/login" element={<LoginPage />} />

      {/* Admin — protected; literal segment takes priority over /:category */}
      <Route
        path="/admin"
        element={
          <ProtectedRoute>
            <AdminApp />
          </ProtectedRoute>
        }
      />

      {/* Category pages — must come last */}
      <Route path="/:category" element={<BlogCategory />} />
    </Routes>
  )
}

export default App
