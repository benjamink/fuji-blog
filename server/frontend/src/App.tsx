import { Routes, Route } from 'react-router-dom'
import { BlogHome } from './pages/BlogHome'
import { BlogCategory } from './pages/BlogCategory'
import { BlogPost } from './pages/BlogPost'
import { StatsPage } from './pages/StatsPage'
import AdminApp from './AdminApp'

function App() {
  return (
    <Routes>
      {/* Public blog */}
      <Route path="/" element={<BlogHome />} />
      <Route path="/post/:slug" element={<BlogPost />} />

      {/* Stats — literal segment takes priority over /:category */}
      <Route path="/stats" element={<StatsPage />} />

      {/* Admin — literal segment takes priority over /:category */}
      <Route path="/admin" element={<AdminApp />} />

      {/* Category pages — must come last */}
      <Route path="/:category" element={<BlogCategory />} />
    </Routes>
  )
}

export default App
