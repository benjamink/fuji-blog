import React, { useEffect, useRef, useState } from 'react'
import jsQR from 'jsqr'
import { blogAPI, ApiKeyInfo } from '../api'

function extractKey(raw: string): string | null {
  const normalized = raw.trim().toLowerCase()
  const match = normalized.match(/[0-9a-f]{10}/)
  return match ? match[0] : null
}

/* A QR symbol: three finder patterns plus scattered modules. Inline rather
   than an icon font so the panel stays dependency-free. */
function QrGlyph() {
  return (
    <svg
      className="qr-scan-btn__icon"
      viewBox="0 0 32 32"
      fill="currentColor"
      aria-hidden="true"
      focusable="false"
    >
      <path
        d="M3 3h10v10H3V3zm2 2v6h6V5H5zm2 2h2v2H7V7z
           M19 3h10v10H19V3zm2 2v6h6V5h-6zm2 2h2v2h-2V7z
           M3 19h10v10H3V19zm2 2v6h6v-6H5zm2 2h2v2H7v-2z"
      />
      <path d="M16 3h2v4h-2V3zm0 6h2v4h-2V9zm3 8h4v2h-4v-2z" />
      <path d="M16 16h2v2h-2v-2zm9 0h4v2h-4v-2zm2 3h2v4h-2v-4zm-6 0h2v2h-2v-2zm-5 3h2v3h-2v-3zm5 3h2v2h-2v-2zm5 0h4v2h-4v-2zm-9 2h2v2h-2v-2z" />
    </svg>
  )
}

/**
 * Read one video frame and return any QR payload in it.
 *
 * Chrome/Edge on Android expose BarcodeDetector, which decodes on the GPU and
 * is much cheaper; Safari and Firefox do not, and a phone browser is exactly
 * where this feature gets used, so jsQR decodes a canvas snapshot as fallback.
 */
async function readFrame(
  video: HTMLVideoElement,
  detector: any | null,
  canvas: HTMLCanvasElement | null,
): Promise<string | null> {
  if (video.readyState < video.HAVE_CURRENT_DATA) return null

  if (detector) {
    const codes = await detector.detect(video)
    return codes.length > 0 ? codes[0].rawValue ?? null : null
  }

  const width = video.videoWidth
  const height = video.videoHeight
  if (!canvas || !width || !height) return null
  canvas.width = width
  canvas.height = height
  const ctx = canvas.getContext('2d', { willReadFrequently: true })
  if (!ctx) return null
  ctx.drawImage(video, 0, 0, width, height)
  const found = jsQR(ctx.getImageData(0, 0, width, height).data, width, height)
  return found ? found.data : null
}

/**
 * Admin panel for the Apple II client's pre-shared API key.
 *
 * The Apple II client can't send an Authorization header, so it authenticates by
 * appending this key as ?key= to admin requests.  Here the admin can view the
 * active key, generate/rotate it, or import it from a scanned QR code.
 */
export function ApiKeyPanel() {
  const [info, setInfo] = useState<ApiKeyInfo | null>(null)
  const [loading, setLoading] = useState(true)
  const [error, setError] = useState<string | null>(null)
  const [copied, setCopied] = useState(false)
  const [importValue, setImportValue] = useState('')
  const [importing, setImporting] = useState(false)
  const [scanActive, setScanActive] = useState(false)
  const [scanError, setScanError] = useState<string | null>(null)
  const videoRef = useRef<HTMLVideoElement | null>(null)
  const canvasRef = useRef<HTMLCanvasElement | null>(null)

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
          'every Apple II client must be updated with the new key.',
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

  const supportsCamera =
    typeof navigator !== 'undefined' &&
    !!navigator.mediaDevices &&
    typeof navigator.mediaDevices.getUserMedia === 'function'

  const importKey = async (rawText: string) => {
    const key = extractKey(rawText)
    if (!key) {
      setError('A valid 10-character hex API key is required.')
      return
    }
    setImporting(true)
    setError(null)
    setCopied(false)
    try {
      setInfo(await blogAPI.importApiKey(key))
      setImportValue('')
      setScanError(null)
    } catch {
      setError('Failed to import API key')
    } finally {
      setImporting(false)
    }
  }

  /* The camera is driven from an effect rather than the click handler so the
     <video> element is guaranteed to be mounted before the stream is attached
     — starting it inline attaches to a ref that React has not yet populated. */
  useEffect(() => {
    if (!scanActive) return

    let cancelled = false
    let timer: number | null = null
    let stream: MediaStream | null = null

    const run = async () => {
      try {
        stream = await navigator.mediaDevices.getUserMedia({
          video: { facingMode: 'environment' },
        })
        const video = videoRef.current
        if (cancelled || !video) return
        video.srcObject = stream
        await video.play()

        const detector =
          typeof (window as any).BarcodeDetector === 'function'
            ? new (window as any).BarcodeDetector({ formats: ['qr_code'] })
            : null

        const tick = async () => {
          if (cancelled) return
          try {
            const text = await readFrame(video, detector, canvasRef.current)
            if (text !== null) {
              const key = extractKey(text)
              if (key) {
                setScanActive(false)
                await importKey(key)
                return
              }
              setScanError('Found a QR code, but not a 10-character hex key.')
            }
          } catch {
            setScanError('QR scan failed — enter the key by hand instead.')
            setScanActive(false)
            return
          }
          /* ~7 fps: fast enough to feel instant, slow enough that the jsQR
             fallback does not peg a phone CPU. */
          timer = window.setTimeout(tick, 150)
        }
        tick()
      } catch {
        if (!cancelled) {
          setScanError('Camera access denied or unavailable.')
          setScanActive(false)
        }
      }
    }
    run()

    return () => {
      cancelled = true
      if (timer !== null) window.clearTimeout(timer)
      if (stream) stream.getTracks().forEach((track) => track.stop())
    }
  }, [scanActive])

  const handleImport = async () => {
    await importKey(importValue)
  }

  const sourceNote = (s: ApiKeyInfo['source']) =>
    s === 'file'
      ? 'Active key was generated here and stored on the server.'
      : s === 'env'
      ? 'Active key comes from the API_KEY environment variable. Generating ' +
        'one here will override it without a restart.'
      : 'No key configured yet. Generate one to let the Apple II client ' +
        'authenticate.'

  return (
    <div className="apikey-panel" style={{ maxWidth: 640 }}>
      <h2 style={{ marginTop: 0 }}>Apple II Client API Key</h2>
      <p style={{ color: '#555', lineHeight: 1.5 }}>
        The Apple II client sends this key as <code>?key=</code> on admin
        requests (it cannot send auth headers). The easy way to set it: on the
        client choose <strong>Configuration → Generate Key + QR</strong>, then
        hit <strong>Scan QR code</strong> below and point the camera at the
        Apple II screen. Generating here instead means typing the key into{' '}
        <strong>Configuration → API Key</strong> by hand.
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

          <div style={{ marginTop: 16, display: 'grid', gap: 10 }}>
            <label style={{ fontSize: 13, color: '#666' }}>
              Take a key from the Apple II
            </label>
            <div className="qr-scan-row">
              <button
                className={`qr-scan-btn${scanActive ? ' is-scanning' : ''}`}
                onClick={() => {
                  setScanError(null)
                  setScanActive(true)
                }}
                disabled={!supportsCamera || scanActive || importing}
              >
                <QrGlyph />
                <span>
                  <span className="qr-scan-btn__label">
                    {scanActive ? 'Scanning…' : 'Scan QR code'}
                  </span>
                  <span className="qr-scan-btn__hint">
                    {scanActive
                      ? 'Looking for a code — hold the camera steady'
                      : 'Point the camera at the Apple II screen'}
                  </span>
                </span>
              </button>
              {scanActive && (
                <button className="nav-btn" onClick={() => setScanActive(false)}>
                  Stop scan
                </button>
              )}
              {!supportsCamera && (
                <span style={{ color: '#999', fontSize: 12 }}>
                  No camera available in this browser — type the key below.
                </span>
              )}
            </div>

            {scanActive && (
              <div>
                <video
                  ref={videoRef}
                  muted
                  playsInline
                  autoPlay
                  style={{
                    width: '100%',
                    maxHeight: 320,
                    borderRadius: 8,
                    background: '#000',
                    objectFit: 'contain',
                  }}
                />
                <p style={{ fontSize: 12, color: '#777', margin: '6px 0 0' }}>
                  Fill the frame with the Apple II screen and hold steady.
                </p>
              </div>
            )}
            <canvas ref={canvasRef} style={{ display: 'none' }} />

            {scanError && (
              <div className="error-banner" style={{ marginTop: 0 }}>
                {scanError}
              </div>
            )}

            <div style={{ display: 'flex', gap: 8, flexWrap: 'wrap' }}>
              <input
                type="text"
                value={importValue}
                onChange={(event) => setImportValue(event.target.value)}
                placeholder="…or type the 10-character key shown on the Apple II"
                style={{ flex: 1, minWidth: 220, padding: '8px 10px', borderRadius: 6, border: '1px solid #ddd' }}
              />
              <button
                className="nav-btn"
                onClick={handleImport}
                disabled={importing || !importValue}
              >
                {importing ? 'Saving…' : 'Use this key'}
              </button>
            </div>
          </div>

          <p style={{ fontSize: 12, color: '#999', marginTop: 16 }}>
            Keys are 10 characters — short enough to type by hand on the Apple
            II. The key can appear in server access logs — use HTTPS on
            untrusted networks.
          </p>
        </>
      )}
    </div>
  )
}
