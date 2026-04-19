// Flora Executor Web UI - JavaScript

class FloraExecutor {
    constructor() {
        this.attached = false;
        this.settings = {
            alwaysOnTop: false,
            textSize: 14,
            autoAttach: false
        };
        this.init();
    }

    init() {
        this.bindEvents();
        this.loadSettings();
        this.setupDraggable();
    }

    bindEvents() {
        // Window controls
        document.getElementById('minimizeBtn').addEventListener('click', () => this.minimize());
        document.getElementById('closeBtn').addEventListener('click', () => this.close());

        // Tab navigation
        document.querySelectorAll('.tab-button[data-tab]').forEach(btn => {
            btn.addEventListener('click', (e) => this.switchTab(e.currentTarget.dataset.tab));
        });

        // Discord button
        document.getElementById('discordBtn').addEventListener('click', () => {
            this.openDiscord();
        });

        // Action buttons
        document.getElementById('attachBtn').addEventListener('click', () => this.attach());
        document.getElementById('detachBtn').addEventListener('click', () => this.detach());
        document.getElementById('clearBtn').addEventListener('click', () => this.clear());
        document.getElementById('executeBtn').addEventListener('click', () => this.execute());
        document.getElementById('redirBtn').addEventListener('click', () => this.redirConsole());

        // Settings
        document.getElementById('alwaysOnTop').addEventListener('change', (e) => {
            this.settings.alwaysOnTop = e.target.checked;
            this.saveSettings();
            this.sendToBackend({ type: 'setAlwaysOnTop', value: e.target.checked });
        });

        document.getElementById('textSize').addEventListener('input', (e) => {
            this.settings.textSize = parseFloat(e.target.value);
            document.getElementById('textSizeValue').textContent = e.target.value;
            this.saveSettings();
            this.sendToBackend({ type: 'setTextSize', value: e.target.value });
        });

        document.getElementById('autoAttach').addEventListener('change', (e) => {
            this.settings.autoAttach = e.target.checked;
            this.saveSettings();
            this.sendToBackend({ type: 'setAutoAttach', value: e.target.checked });
        });

        document.getElementById('resetSettingsBtn').addEventListener('click', () => this.resetSettings());

        // Log terminal toggle
        document.getElementById('toggleLogBtn').addEventListener('click', () => {
            const logTerminal = document.getElementById('logTerminal');
            logTerminal.classList.toggle('collapsed');
        });
    }

    setupDraggable() {
        const titleBar = document.getElementById('titleBar');
        let isDragging = false;
        let offsetX, offsetY;

        titleBar.addEventListener('mousedown', (e) => {
            if (e.target.closest('.window-button')) return;
            isDragging = true;
            offsetX = e.clientX;
            offsetY = e.clientY;
        });

        document.addEventListener('mousemove', (e) => {
            if (!isDragging) return;
            const deltaX = e.clientX - offsetX;
            const deltaY = e.clientY - offsetY;
            this.sendToBackend({ type: 'moveWindow', deltaX, deltaY });
            offsetX = e.clientX;
            offsetY = e.clientY;
        });

        document.addEventListener('mouseup', () => {
            isDragging = false;
        });
    }

    switchTab(tabId) {
        document.querySelectorAll('.tab-button').forEach(btn => {
            btn.classList.toggle('active', btn.dataset.tab === tabId);
        });
        document.querySelectorAll('.tab-panel').forEach(panel => {
            panel.classList.toggle('active', panel.id === tabId);
        });
    }

    minimize() {
        this.sendToBackend({ type: 'minimize' });
    }

    close() {
        this.sendToBackend({ type: 'close' });
    }

    openDiscord() {
        this.sendToBackend({ type: 'openDiscord' });
    }

    attach() {
        this.sendToBackend({ type: 'attach' });
    }

    detach() {
        this.sendToBackend({ type: 'detach' });
    }

    clear() {
        document.getElementById('scriptEditor').value = '';
    }

    execute() {
        const script = document.getElementById('scriptEditor').value;
        if (!script.trim()) {
            this.showNotification('warning', 'Warning', 'No script to execute');
            return;
        }
        this.sendToBackend({ type: 'execute', script });
    }

    redirConsole() {
        this.sendToBackend({ type: 'redirConsole' });
    }

    loadSettings() {
        this.sendToBackend({ type: 'loadSettings' });
    }

    saveSettings() {
        this.sendToBackend({ type: 'saveSettings', settings: this.settings });
    }

    resetSettings() {
        this.settings = {
            alwaysOnTop: false,
            textSize: 14,
            autoAttach: false
        };
        document.getElementById('alwaysOnTop').checked = false;
        document.getElementById('textSize').value = 14;
        document.getElementById('textSizeValue').textContent = '14';
        document.getElementById('autoAttach').checked = false;
        this.saveSettings();
        this.showNotification('info', 'Settings', 'Settings reset to default');
    }

    sendToBackend(message) {
        // This will be connected to WebView2's postMessage
        if (window.chrome && window.chrome.webview) {
            window.chrome.webview.postMessage(JSON.stringify(message));
        } else {
            console.log('Backend message:', message);
        }
    }

    receiveFromBackend(message) {
        switch (message.type) {
            case 'attached':
                this.attached = true;
                this.updateStatus(true);
                this.showNotification('success', 'Attached', 'Successfully attached to Roblox');
                break;
            case 'detached':
                this.attached = false;
                this.updateStatus(false);
                this.showNotification('info', 'Detached', 'Detached from Roblox');
                break;
            case 'executeResult':
                if (message.success) {
                    this.showNotification('success', 'Executed', 'Script executed successfully');
                } else {
                    this.showNotification('error', 'Error', message.error || 'Execution failed');
                }
                break;
            case 'log':
                this.addLog(message.level, message.message);
                break;
            case 'settings':
                this.settings = message.settings;
                document.getElementById('alwaysOnTop').checked = this.settings.alwaysOnTop;
                document.getElementById('textSize').value = this.settings.textSize;
                document.getElementById('textSizeValue').textContent = this.settings.textSize;
                document.getElementById('autoAttach').checked = this.settings.autoAttach;
                break;
        }
    }

    updateStatus(attached) {
        const statusDot = document.querySelector('.status-dot');
        const statusText = document.getElementById('statusText');
        statusDot.classList.toggle('attached', attached);
        statusText.textContent = attached ? 'Attached' : 'Not attached';
    }

    addLog(level, message) {
        const logContent = document.getElementById('logContent');
        const logEntry = document.createElement('div');
        logEntry.className = `log-entry ${level}`;
        logEntry.textContent = `[${new Date().toLocaleTimeString()}] ${message}`;
        logContent.appendChild(logEntry);
        logContent.scrollTop = logContent.scrollHeight;
    }

    showNotification(type, title, message) {
        const notifications = document.getElementById('notifications');
        const notification = document.createElement('div');
        notification.className = 'notification';
        
        const icons = {
            success: `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M22 11.08V12a10 10 0 1 1-5.93-9.14"></path><polyline points="22 4 12 14.01 9 11.01"></polyline></svg>`,
            error: `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><circle cx="12" cy="12" r="10"></circle><line x1="15" y1="9" x2="9" y2="15"></line><line x1="9" y1="9" x2="15" y2="15"></line></svg>`,
            warning: `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M10.29 3.86L1.82 18a2 2 0 0 0 1.71 3h16.94a2 2 0 0 0 1.71-3L13.71 3.86a2 2 0 0 0-3.42 0z"></path><line x1="12" y1="9" x2="12" y2="13"></line><line x1="12" y1="17" x2="12.01" y2="17"></line></svg>`,
            info: `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><circle cx="12" cy="12" r="10"></circle><line x1="12" y1="16" x2="12" y2="12"></line><line x1="12" y1="8" x2="12.01" y2="8"></line></svg>`
        };

        notification.innerHTML = `
            <div class="notification-content">
                <div class="notification-icon ${type}">
                    ${icons[type]}
                </div>
                <div class="notification-text">
                    <div class="notification-title">${title}</div>
                    <div class="notification-message">${message}</div>
                </div>
            </div>
            <button class="notification-close">
                <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
                    <line x1="18" y1="6" x2="6" y2="18"></line>
                    <line x1="6" y1="6" x2="18" y2="18"></line>
                </svg>
            </button>
            <div class="notification-progress"></div>
        `;

        const closeBtn = notification.querySelector('.notification-close');
        closeBtn.addEventListener('click', () => {
            notification.classList.add('slide-out');
            setTimeout(() => notification.remove(), 300);
        });

        notifications.appendChild(notification);

        setTimeout(() => {
            notification.classList.add('slide-out');
            setTimeout(() => notification.remove(), 300);
        }, 3000);
    }
}

// Initialize when DOM is ready
document.addEventListener('DOMContentLoaded', () => {
    window.flora = new FloraExecutor();
});

// Handle messages from WebView2 backend
window.addEventListener('message', (event) => {
    try {
        const message = JSON.parse(event.data);
        if (window.flora) {
            window.flora.receiveFromBackend(message);
        }
    } catch (e) {
        console.error('Failed to parse message from backend:', e);
    }
});
