/**
 * Delta CLI Install Tracking Dashboard
 * Node.js server with Express for tracking anonymous install statistics
 */

const express = require('express');
const fs = require('fs').promises;
const path = require('path');
const app = express();

// Configuration
const PORT = process.env.PORT || 3000;
const DB_FILE = path.join(__dirname, 'db.json');

// Middleware
app.use(express.json());
app.use(express.static(path.join(__dirname, 'public')));

// CORS for local development
app.use((req, res, next) => {
    res.header('Access-Control-Allow-Origin', '*');
    res.header('Access-Control-Allow-Headers', 'Origin, X-Requested-With, Content-Type, Accept');
    next();
});

// Initialize database
async function initDB() {
    try {
        await fs.access(DB_FILE);
    } catch {
        const initialData = {
            installs: [],
            stats: {
                total: 0,
                platforms: {}
            }
        };
        await fs.writeFile(DB_FILE, JSON.stringify(initialData, null, 2));
    }
}

// Read database
async function readDB() {
    try {
        const data = await fs.readFile(DB_FILE, 'utf8');
        return JSON.parse(data);
    } catch (err) {
        console.error('Error reading database:', err);
        return { installs: [], stats: { total: 0, platforms: {} } };
    }
}

// Write database
async function writeDB(data) {
    try {
        await fs.writeFile(DB_FILE, JSON.stringify(data, null, 2));
    } catch (err) {
        console.error('Error writing database:', err);
    }
}

// Update statistics
function updateStats(db) {
    db.stats.total = db.installs.length;
    db.stats.platforms = {};
    
    for (const install of db.installs) {
        const platform = install.platform || 'Unknown';
        db.stats.platforms[platform] = (db.stats.platforms[platform] || 0) + 1;
    }
    
    return db;
}

// Routes

// Track install
app.post('/track', async (req, res) => {
    try {
        const { uuid, platform } = req.body;
        
        if (!uuid) {
            return res.status(400).json({ error: 'UUID required' });
        }
        
        const db = await readDB();
        
        // Check if UUID already exists
        const existing = db.installs.find(i => i.uuid === uuid);
        if (existing) {
            // Update timestamp
            existing.lastSeen = new Date().toISOString();
            existing.platform = platform || existing.platform;
        } else {
            // Add new install
            db.installs.push({
                uuid,
                platform: platform || 'Unknown',
                firstSeen: new Date().toISOString(),
                lastSeen: new Date().toISOString()
            });
        }
        
        updateStats(db);
        await writeDB(db);
        
        res.json({ success: true, total: db.stats.total });
    } catch (err) {
        console.error('Error tracking install:', err);
        res.status(500).json({ error: 'Internal server error' });
    }
});

// Get statistics
app.get('/stats', async (req, res) => {
    try {
        const db = await readDB();
        updateStats(db);
        
        // Calculate additional metrics
        const now = new Date();
        const last24h = db.installs.filter(i => {
            const lastSeen = new Date(i.lastSeen);
            return (now - lastSeen) < 24 * 60 * 60 * 1000;
        }).length;
        
        const last7d = db.installs.filter(i => {
            const lastSeen = new Date(i.lastSeen);
            return (now - lastSeen) < 7 * 24 * 60 * 60 * 1000;
        }).length;
        
        res.json({
            total: db.stats.total,
            platforms: db.stats.platforms,
            active24h: last24h,
            active7d: last7d,
            installs: db.installs.map(i => ({
                platform: i.platform,
                firstSeen: i.firstSeen,
                lastSeen: i.lastSeen
            }))
        });
    } catch (err) {
        console.error('Error getting stats:', err);
        res.status(500).json({ error: 'Internal server error' });
    }
});

// Dashboard UI
app.get('/', (req, res) => {
    res.sendFile(path.join(__dirname, 'public', 'index.html'));
});

// Health check
app.get('/health', (req, res) => {
    res.json({ status: 'ok', timestamp: new Date().toISOString() });
});

// Start server
initDB().then(() => {
    app.listen(PORT, () => {
        console.log(`
╔══════════════════════════════════════════════════════════════╗
║              Delta CLI Install Tracking Dashboard           ║
╚══════════════════════════════════════════════════════════════╝

Server running at: http://localhost:${PORT}
Database: ${DB_FILE}

Endpoints:
  POST /track      - Track an install (body: {uuid, platform})
  GET  /stats      - Get install statistics
  GET  /           - Dashboard UI
  GET  /health     - Health check

Press Ctrl+C to stop
        `);
    });
}).catch(err => {
    console.error('Failed to initialize database:', err);
    process.exit(1);
});

