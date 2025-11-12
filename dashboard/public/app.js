// Dashboard JavaScript
let platformChart = null;
let platformPieChart = null;

// Format date
function formatDate(dateString) {
    const date = new Date(dateString);
    const now = new Date();
    const diff = now - date;
    
    // Less than 1 minute
    if (diff < 60000) {
        return 'Just now';
    }
    // Less than 1 hour
    if (diff < 3600000) {
        const mins = Math.floor(diff / 60000);
        return `${mins} minute${mins > 1 ? 's' : ''} ago`;
    }
    // Less than 1 day
    if (diff < 86400000) {
        const hours = Math.floor(diff / 3600000);
        return `${hours} hour${hours > 1 ? 's' : ''} ago`;
    }
    // Less than 1 week
    if (diff < 604800000) {
        const days = Math.floor(diff / 86400000);
        return `${days} day${days > 1 ? 's' : ''} ago`;
    }
    
    return date.toLocaleDateString();
}

// Platform colors
const platformColors = {
    'Linux': '#00ff00',
    'macOS': '#00cc00',
    'Windows': '#00aa00',
    'Android': '#008800',
    'iOS': '#006600',
    'Unknown': '#004400'
};

// Load data from API
async function loadData() {
    const btn = document.getElementById('refresh-btn');
    btn.disabled = true;
    btn.textContent = 'Loading...';
    
    try {
        const response = await fetch('/stats');
        const data = await response.json();
        
        // Update stats cards
        document.getElementById('total-installs').textContent = data.total || 0;
        document.getElementById('active-24h').textContent = data.active24h || 0;
        document.getElementById('active-7d').textContent = data.active7d || 0;
        document.getElementById('platform-count').textContent = 
            Object.keys(data.platforms || {}).length;
        
        // Update platform charts
        updatePlatformChart(data.platforms || {});
        updatePlatformPie(data.platforms || {});
        
        // Update installs table
        updateInstallsTable(data.installs || []);
        
        // Update last updated time
        document.getElementById('last-updated').textContent = 
            `Last updated: ${new Date().toLocaleTimeString()}`;
        
    } catch (err) {
        console.error('Error loading data:', err);
        document.getElementById('installs-tbody').innerHTML = 
            '<tr><td colspan="3" class="error">Failed to load data</td></tr>';
    } finally {
        btn.disabled = false;
        btn.textContent = 'Refresh Data';
    }
}

// Update platform bar chart
function updatePlatformChart(platforms) {
    const ctx = document.getElementById('platform-chart');
    
    const labels = Object.keys(platforms).sort();
    const data = labels.map(platform => platforms[platform]);
    const colors = labels.map(platform => platformColors[platform] || platformColors['Unknown']);
    
    if (platformChart) {
        platformChart.destroy();
    }
    
    platformChart = new Chart(ctx, {
        type: 'bar',
        data: {
            labels: labels,
            datasets: [{
                label: 'Installs',
                data: data,
                backgroundColor: colors.map(c => c + '33'),
                borderColor: colors,
                borderWidth: 2
            }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: true,
            scales: {
                y: {
                    beginAtZero: true,
                    ticks: { 
                        color: '#00ff00',
                        stepSize: 1
                    },
                    grid: { color: 'rgba(0, 255, 0, 0.1)' }
                },
                x: {
                    ticks: { color: '#00ff00' },
                    grid: { color: 'rgba(0, 255, 0, 0.1)' }
                }
            },
            plugins: {
                legend: {
                    labels: { color: '#00ff00' }
                }
            }
        }
    });
}

// Update platform pie chart
function updatePlatformPie(platforms) {
    const ctx = document.getElementById('platform-pie');
    
    const labels = Object.keys(platforms).sort();
    const data = labels.map(platform => platforms[platform]);
    const colors = labels.map(platform => platformColors[platform] || platformColors['Unknown']);
    
    if (platformPieChart) {
        platformPieChart.destroy();
    }
    
    platformPieChart = new Chart(ctx, {
        type: 'doughnut',
        data: {
            labels: labels,
            datasets: [{
                data: data,
                backgroundColor: colors.map(c => c + '66'),
                borderColor: colors,
                borderWidth: 2
            }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: true,
            plugins: {
                legend: {
                    position: 'bottom',
                    labels: { 
                        color: '#00ff00',
                        padding: 15
                    }
                }
            }
        }
    });
}

// Update installs table
function updateInstallsTable(installs) {
    const tbody = document.getElementById('installs-tbody');
    
    if (installs.length === 0) {
        tbody.innerHTML = '<tr><td colspan="3" class="loading">No installs yet</td></tr>';
        return;
    }
    
    // Sort by last seen (most recent first)
    const sorted = installs.sort((a, b) => 
        new Date(b.lastSeen) - new Date(a.lastSeen)
    );
    
    // Show only last 20
    const recent = sorted.slice(0, 20);
    
    tbody.innerHTML = recent.map(install => `
        <tr>
            <td><span class="platform-badge">${install.platform}</span></td>
            <td>${formatDate(install.firstSeen)}</td>
            <td>${formatDate(install.lastSeen)}</td>
        </tr>
    `).join('');
}

// Auto-refresh every 30 seconds
setInterval(loadData, 30000);

// Load data on page load
loadData();

