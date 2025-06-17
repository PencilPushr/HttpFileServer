class FileServerApp {
    constructor() {
        this.currentPath = '';
        this.currentView = 'list';
        this.init();
    }

    init() {
        this.bindEvents();
        this.loadStats();
        this.loadFiles();
        this.setupDragAndDrop();
    }

    bindEvents() {
        // Upload functionality
        document.getElementById('fileInput').addEventListener('change', (e) => {
            this.handleFileSelect(e.target.files);
        });

        document.getElementById('uploadBtn').addEventListener('click', () => {
            this.triggerFileSelect();
        });

        // View controls
        document.getElementById('listViewBtn').addEventListener('click', () => {
            this.setView('list');
        });

        document.getElementById('gridViewBtn').addEventListener('click', () => {
            this.setView('grid');
        });

        // Refresh button
        document.getElementById('refreshBtn').addEventListener('click', () => {
            this.refresh();
        });

        // Modal controls
        document.getElementById('modalCancel').addEventListener('click', () => {
            this.hideModal();
        });

        document.getElementById('modalConfirm').addEventListener('click', () => {
            this.confirmAction();
        });

        // Close modal on background click
        document.getElementById('modal').addEventListener('click', (e) => {
            if (e.target.id === 'modal') {
                this.hideModal();
            }
        });

        // Keyboard shortcuts
        document.addEventListener('keydown', (e) => {
            if (e.key === 'Escape') {
                this.hideModal();
            } else if (e.key === 'F5' || (e.ctrlKey && e.key === 'r')) {
                e.preventDefault();
                this.refresh();
            }
        });
    }

    setupDragAndDrop() {
        const uploadArea = document.getElementById('uploadArea');

        ['dragenter', 'dragover', 'dragleave', 'drop'].forEach(eventName => {
            uploadArea.addEventListener(eventName, this.preventDefaults, false);
            document.body.addEventListener(eventName, this.preventDefaults, false);
        });

        ['dragenter', 'dragover'].forEach(eventName => {
            uploadArea.addEventListener(eventName, () => {
                uploadArea.classList.add('dragover');
            }, false);
        });

        ['dragleave', 'drop'].forEach(eventName => {
            uploadArea.addEventListener(eventName, () => {
                uploadArea.classList.remove('dragover');
            }, false);
        });

        uploadArea.addEventListener('drop', (e) => {
            const files = e.dataTransfer.files;
            this.handleFileSelect(files);
        }, false);
    }

    preventDefaults(e) {
        e.preventDefault();
        e.stopPropagation();
    }

    async loadStats() {
        try {
            const response = await fetch('/api/stats');
            const stats = await response.json();
            this.displayStats(stats);
        } catch (error) {
            console.error('Failed to load stats:', error);
        }
    }

    displayStats(stats) {
        const statsContainer = document.getElementById('statsContainer');
        statsContainer.innerHTML = `
            <div class="stat-item">
                <div class="stat-number">${stats.total_files || 0}</div>
                <div class="stat-label">Total Files</div>
            </div>
            <div class="stat-item">
                <div class="stat-number">${stats.total_size_formatted || '0 B'}</div>
                <div class="stat-label">Storage Used</div>
            </div>
            <div class="stat-item">
                <div class="stat-number">${stats.total_folders || 0}</div>
                <div class="stat-label">Folders</div>
            </div>
        `;
    }

    async loadFiles(path = '') {
        try {
            this.showLoading();
            const response = await fetch(`/api/files?path=${encodeURIComponent(path)}`);
            
            if (!response.ok) {
                throw new Error(`HTTP ${response.status}: ${response.statusText}`);
            }
            
            const files = await response.json();
            this.currentPath = path;
            this.displayFiles(files);
            this.updateBreadcrumb(path);
        } catch (error) {
            console.error('Failed to load files:', error);
            this.showError('Failed to load files: ' + error.message);
        }
    }

    displayFiles(files) {
        const fileList = document.getElementById('fileList');
        
        if (files.length === 0) {
            fileList.innerHTML = `
                <div class="empty-state">
                    <div class="empty-state-icon">📂</div>
                    <h3>No files found</h3>
                    <p>Upload some files to get started!</p>
                </div>
            `;
            return;
        }

        // Sort files: directories first, then by name
        files.sort((a, b) => {
            if (a.is_directory !== b.is_directory) {
                return b.is_directory - a.is_directory;
            }
            return a.name.localeCompare(b.name);
        });

        const fileItems = files.map(file => this.createFileItem(file)).join('');
        fileList.innerHTML = fileItems;
    }

    createFileItem(file) {
        const icon = this.getFileIcon(file);
        const sizeDisplay = file.is_directory ? 'Folder' : file.size_formatted;
        
        return `
            <div class="file-item" data-file="${file.name}" data-is-directory="${file.is_directory}">
                <div class="file-info" ${file.is_directory ? `onclick="app.openFolder('${file.path}')"` : ''}>
                    <span class="file-icon">${icon}</span>
                    <div class="file-details">
                        <div class="file-name">${this.escapeHtml(file.name)}</div>
                        <div class="file-meta">
                            <span>📊 ${sizeDisplay}</span>
                            <span>🕒 ${file.modified}</span>
                            <span>🏷️ ${file.mime_type}</span>
                        </div>
                    </div>
                </div>
                <div class="file-actions">
                    ${file.is_directory ? 
                        `<button onclick="app.openFolder('${file.path}')" class="btn btn-success btn-small">📂 Open</button>` :
                        `<button onclick="app.downloadFile('${file.path}')" class="btn btn-success btn-small">⬇️ Download</button>`
                    }
                    <button onclick="app.confirmDelete('${file.path}', '${this.escapeHtml(file.name)}')" class="btn btn-danger btn-small">🗑️ Delete</button>
                </div>
            </div>
        `;
    }

    getFileIcon(file) {
        if (file.is_directory) return '📁';
        
        const ext = file.name.toLowerCase().split('.').pop();
        const iconMap = {
            'jpg': '🖼️', 'jpeg': '🖼️', 'png': '🖼️', 'gif': '🖼️', 'svg': '🖼️',
            'mp4': '🎬', 'avi': '🎬', 'mov': '🎬', 'mkv': '🎬',
            'mp3': '🎵', 'wav': '🎵', 'flac': '🎵', 'aac': '🎵',
            'pdf': '📄', 'doc': '📄', 'docx': '📄', 'txt': '📋',
            'xls': '📊', 'xlsx': '📊', 'csv': '📊',
            'zip': '📦', 'rar': '📦', '7z': '📦', 'tar': '📦',
            'js': '📜', 'html': '📜', 'css': '📜', 'json': '📜',
            'cpp': '💻', 'c': '💻', 'py': '💻', 'java': '💻'
        };
        
        return iconMap[ext] || '📄';
    }

    updateBreadcrumb(path) {
        const breadcrumb = document.getElementById('breadcrumb');
        const parts = path ? path.split('/').filter(p => p) : [];
        
        let breadcrumbHtml = '<span class="breadcrumb-item" onclick="app.loadFiles(\'\')">🏠 Home</span>';
        
        let currentPath = '';
        parts.forEach((part, index) => {
            currentPath += (currentPath ? '/' : '') + part;
            const isLast = index === parts.length - 1;
            breadcrumbHtml += `<span class="breadcrumb-item ${isLast ? 'active' : ''}" 
                                onclick="app.loadFiles('${currentPath}')">${this.escapeHtml(part)}</span>`;
        });
        
        breadcrumb.innerHTML = breadcrumbHtml;
    }

    setView(view) {
        this.currentView = view;
        const fileList = document.getElementById('fileList');
        const listBtn = document.getElementById('listViewBtn');
        const gridBtn = document.getElementById('gridViewBtn');
        
        if (view === 'grid') {
            fileList.classList.add('grid-view');
            listBtn.classList.remove('active');
            gridBtn.classList.add('active');
        } else {
            fileList.classList.remove('grid-view');
            listBtn.classList.add('active');
            gridBtn.classList.remove('active');
        }
    }

    triggerFileSelect() {
        document.getElementById('fileInput').click();
    }

    handleFileSelect(files) {
        if (files.length === 0) return;
        
        this.showToast(`Selected ${files.length} file(s) for upload`, 'success');
        // For now, just show a message. Upload functionality would be implemented here.
        // You would need to implement multipart form data parsing in the C++ backend
        console.log('Files selected for upload:', Array.from(files).map(f => f.name));
    }

    async downloadFile(path) {
        try {
            const response = await fetch(`/api/download?file=${encodeURIComponent(path)}`);
            
            if (!response.ok) {
                throw new Error(`Download failed: ${response.statusText}`);
            }
            
            const blob = await response.blob();
            const url = window.URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.href = url;
            a.download = path.split('/').pop();
            document.body.appendChild(a);
            a.click();
            document.body.removeChild(a);
            window.URL.revokeObjectURL(url);
            
            this.showToast('Download started', 'success');
        } catch (error) {
            console.error('Download failed:', error);
            this.showError('Download failed: ' + error.message);
        }
    }

    openFolder(path) {
        this.loadFiles(path);
    }

    confirmDelete(path, name) {
        this.pendingAction = {
            type: 'delete',
            path: path,
            name: name
        };
        
        this.showModal(
            'Confirm Delete',
            `Are you sure you want to delete "${name}"? This action cannot be undone.`
        );
    }

    async deleteFile(path) {
        try {
            const response = await fetch(`/api/delete?file=${encodeURIComponent(path)}`, {
                method: 'DELETE'
            });
            
            if (!response.ok) {
                throw new Error(`Delete failed: ${response.statusText}`);
            }
            
            this.showToast('File deleted successfully', 'success');
            this.refresh();
        } catch (error) {
            console.error('Delete failed:', error);
            this.showError('Delete failed: ' + error.message);
        }
    }

    confirmAction() {
        if (this.pendingAction) {
            switch (this.pendingAction.type) {
                case 'delete':
                    this.deleteFile(this.pendingAction.path);
                    break;
            }
            this.pendingAction = null;
        }
        this.hideModal();
    }

    showModal(title, message) {
        document.getElementById('modalTitle').textContent = title;
        document.getElementById('modalMessage').textContent = message;
        document.getElementById('modal').classList.remove('hidden');
    }

    hideModal() {
        document.getElementById('modal').classList.add('hidden');
        this.pendingAction = null;
    }

    showToast(message, type = 'info') {
        const container = document.getElementById('toastContainer');
        const toast = document.createElement('div');
        toast.className = `toast ${type}`;
        toast.textContent = message;
        
        container.appendChild(toast);
        
        setTimeout(() => {
            if (toast.parentNode) {
                toast.parentNode.removeChild(toast);
            }
        }, 5000);
    }

    showError(message) {
        this.showToast(message, 'error');
    }

    showLoading() {
        const fileList = document.getElementById('fileList');
        fileList.innerHTML = `
            <div class="loading">
                <div class="spinner"></div>
                <p>Loading files...</p>
            </div>
        `;
    }

    refresh() {
        this.loadStats();
        this.loadFiles(this.currentPath);
        this.showToast('Refreshed', 'success');
    }

    escapeHtml(text) {
        const div = document.createElement('div');
        div.textContent = text;
        return div.innerHTML;
    }
}

// Initialize the app when the page loads
let app;
document.addEventListener('DOMContentLoaded', () => {
    app = new FileServerApp();
});

// Global error handler
window.addEventListener('error', (e) => {
    console.error('JavaScript error:', e.error);
    if (app) {
        app.showError('An unexpected error occurred');
    }
});

// Handle connection errors
window.addEventListener('online', () => {
    if (app) {
        app.showToast('Connection restored', 'success');
    }
});

window.addEventListener('offline', () => {
    if (app) {
        app.showToast('Connection lost', 'warning');
    }
});