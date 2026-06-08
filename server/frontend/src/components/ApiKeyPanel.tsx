import React, { useEffect, useState } from 'react'
import { blogAPI, ApiKeyInfo } from '../api'

/**
 * Admin panel for the Apple IIc client's pre-shared API key.
 *
 * The IIc client can't send an Authorization header, so it authenticates by
 * appending this key as ?key= to admin requests.  Here the admin can view the
 * active key and generate/rotate it.  A generated key is stored server-side and
 * takes precedence over the API_KEY env var.
 */
export function ApiKeyPanel() {
  const [info, setInfo] = useState<ApiKeyInfo | null>(null)
  const [loading, setLoading] = useState(true)
  const [error, setError] = useState<string | null>(null)
  const [copied, setCopied] = useState(false)

  useEffect(() => {
    blogAPI
      .getApiKey()
      .then(setInfo)
      .catch(() => setError('Failed to load API key'))
      .finally(() => setLoading(false))
  }, [])

  const handleGenerate = async () => {
    const replacing = info && info.source !== 'none'
    if (
      replacing &&
      !window.confirm(
        'Generate a new key? The current key stops working immediately and ' +
          'every Apple IIc client must be updated with the new key.',
      )
    ) {
      return
    }
    setLoading(true)
    setError(null)
    setCopied(false)
    try {
      setInfo(await blogAPI.generateApiKey())
    } catch {
      setError('Failed to generate API key')
    } finally {
      setLoading(false)
    }
  }

  const handleCopy = async () => {
    if (!info?.api_key) return
    try {
      await navigator.clipboard.writeText(info.api_key)
      setCopied(true)
      setTimeout(() => setCopied(false), 1500)
    } catch {
      setError('Clipboard copy failed — select and copy the key manually.')
    }
  }

  const sourceNote = (s: ApiKeyInfo['source']) =>
    s === 'file'
      ? 'Active key was generated here and stored on the server.'
      : s === 'env'
      ? 'Active key comes from the API_KEY environment variable. Generating ' +
        'one here will override it without a restart.'
      : 'No key configured yet. Generate one to let the Apple IIc client ' +
        'authenticate.'

  return (
    <div className="apikey-panel" style={{ maxWidth: 640 }}>
      <h2 style={{ marginTop: 0 }}>Apple IIc Client API Key</h2>
      <p style={{ color: '#555', lineHeight: 1.5 }}>
        The Apple IIc client sends this key as <code>?key=</code> on admin
        requests (it cannot send auth headers). Enter it on the client under{' '}
        <strong>Configuration → API Key</strong>; it saves automatically on
        entry.
      </p>

      {error && <div className="error-banner">{error}</div>}

      {loading && !info ? (
        <div className="loading">Loading…</div>
      ) : (
        <>
          <div style={{ margin: '16px 0' }}>
            <label
              style={{ display: 'block', fontSize: 13, color: '#666', marginBottom: 6 }}
            >
              Current key
            </label>
            <div style={{ display: 'flex', gap: 8, alignItems: 'center' }}>
              <code
                style={{
                  flex: 1,
                  padding: '10px 12px',
                  background: '#f4f4f4',
                  border: '1px solid #ddd',
                  borderRadius: 6,
                  fontFamily: 'monospace',
                  fontSize: 14,
                  wordBreak: 'break-all',
                  userSelect: 'all',
                }}
              >
                {info?.api_key || '(none)'}
              </code>
              {info?.api_key && (
                <button className="nav-btn" onClick={handleCopy} disabled={loading}>
                  {copied ? 'Copied!' : 'Copy'}
                </button>
              )}
            </div>
            <p style={{ fontSize: 13, color: '#777', marginTop: 8 }}>
              {info ? sourceNote(info.source) : ''}
            </p>
          </div>

          <button className="nav-btn active" onClick={handleGenerate} disabled={loading}>
            {loading
              ? 'Working…'
              : info && info.source !== 'none'
              ? 'Generate new key'
              : 'Generate key'}
          </button>

          <p style={{ fontSize: 12, color: '#999', marginTop: 16 }}>
            Keys are 10 characters — short enough to type by hand on the Apple
            IIc. The key can appear in server access logs — use HTTPS on
            untrusted networks.
          </p>
        </>
      )}
    </div>
  )
}
