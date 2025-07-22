class FileServerApp {
    constructor() {
        this.currentPath = '';
        this.currentView = 'list';
        this.searchQuery = '';
        this.allFiles = []; // Store all files for client-side search
        this.init();
    }

    init() {
        this.bindEvents();
        this.loadStats();
        this.loadFiles();
        this.setupDragAndDrop();
    }

    debounce(func, wait) {
        let timeout;
        return function executedFunction(...args) {
            const later = () => {
                clearTimeout(timeout);
                func(...args);
            };
            clearTimeout(timeout);
            timeout = setTimeout(later, wait);
        };
    }

    bindEvents() {
        // Upload functionality
        document.getElementById('fileInput').addEventListener('change', (e) => {
            this.handleFileSelect(e.target.files);
        });

        document.getElementById('uploadBtn').addEventListener('click', () => {
            const files = document.getElementById('fileInput').files;
            if (files.length > 0) {
                this.uploadFiles(files);
            } else {
                this.triggerFileSelect();
            }
        });

        // Search functionality
        document.getElementById("searchInput").addEventListener("input", this.debounce(() => {
            this.handleSearch();
        }, 300));

        document.getElementById("searchInput").addEventListener("keydown", (e) => {
            if (e.key === 'Escape') {
                this.clearSearch();
            }
        });

        document.getElementById("clearSearchBtn").addEventListener('click', () =>
            this.clearSearch()
        );

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
            this.allFiles = files; // Store all files for search
            this.displayFiles(files);
            this.updateBreadcrumb(path);

            // Clear search whilst navigating
            this.clearSearch();
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
        const isMediaFile = this.isMediaFile(file.name);
    
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
                        `${isMediaFile ? 
                            `<button onclick="app.playMedia('${file.path}')" class="btn btn-primary btn-small">▶️ Play</button>` : 
                            ''}
                        <button onclick="app.downloadFile('${file.path}')" class="btn btn-success btn-small">⬇️ Download</button>`
                    }
                    <button onclick="app.confirmDelete('${file.path}', '${this.escapeHtml(file.name)}')" class="btn btn-danger btn-small">🗑️ Delete</button>
                </div>
            </div>
        `;
    }

    playMedia(path) {
        // Encode the path properly for URL
        const mediaUrl = `/api/download?file=${encodeURIComponent(path)}`;

        // Open in new tab
        const newWindow = window.open('', '_blank');

        // Create a simple HTML5 media player page
        const ext = path.toLowerCase().substring(path.lastIndexOf('.'));
        const isVideo = ['.mp4', '.mkv', '.avi', '.mov', '.wmv', '.flv', '.webm', '.m4v', '.mpg', '.mpeg', '.3gp', '.ogv', '.vlc', '.vob', '.ts', '.m2ts', '.divx', '.xvid', '.rm', '.rmvb'].includes(ext);
        const isAudio = ['.mp3', '.wav', '.flac', '.aac', '.ogg', '.wma', '.m4a', '.opus'].includes(ext);

        const playerHtml = `
        <!DOCTYPE html>
        <html>
        <head>
            <title>${path.split('/').pop()} - Media Player</title>
            <style>
                body {
                    margin: 0;
                    padding: 0;
                    background: #000;
                    display: flex;
                    justify-content: center;
                    align-items: center;
                    min-height: 100vh;
                    font-family: Arial, sans-serif;
                }
                video, audio {
                    max-width: 100%;
                    max-height: 100vh;
                    width: auto;
                    height: auto;
                }
                .error {
                    color: #fff;
                    text-align: center;
                    padding: 20px;
                }
                .controls {
                    position: fixed;
                    top: 0;
                    left: 0;
                    right: 0;
                    background: rgba(0,0,0,0.8);
                    color: white;
                    padding: 10px;
                    display: flex;
                    justify-content: space-between;
                    align-items: center;
                }
                .title {
                    font-size: 14px;
                    overflow: hidden;
                    text-overflow: ellipsis;
                    white-space: nowrap;
                    flex: 1;
                }
                .close-btn {
                    background: #dc3545;
                    color: white;
                    border: none;
                    padding: 5px 15px;
                    border-radius: 4px;
                    cursor: pointer;
                    margin-left: 10px;
                }
                .close-btn:hover {
                    background: #c82333;
                }
            </style>
        </head>
        <body>
            <div class="controls">
                <span class="title">${path.split('/').pop()}</span>
                <button class="close-btn" onclick="window.close()">Close</button>
            </div>
            ${isVideo ?
                `<video controls autoplay>
                    <source src="${mediaUrl}" type="video/${ext.substring(1)}">
                    <div class="error">Your browser doesn't support this video format. Try downloading the file instead.</div>
                </video>` :
                isAudio ?
                    `<audio controls autoplay>
                    <source src="${mediaUrl}" type="audio/${ext.substring(1)}">
                    <div class="error">Your browser doesn't support this audio format. Try downloading the file instead.</div>
                </audio>` :
                    `<div class="error">Unsupported media format. Try downloading the file instead.</div>`
            }
        </body>
        </html>
    `;

        newWindow.document.write(playerHtml);
        newWindow.document.close();
    }

    isMediaFile(filename) {
        const mediaExtensions = [
            // Video formats
            '.mp4', '.mkv', '.avi', '.mov', '.wmv', '.flv', '.webm', '.m4v', '.mpg', '.mpeg', '.3gp', '.ogv',
            // Audio formats
            '.mp3', '.wav', '.flac', '.aac', '.ogg', '.wma', '.m4a', '.opus',
            // VLC can handle these too
            '.vlc', '.vob', '.ts', '.m2ts', '.divx', '.xvid', '.rm', '.rmvb'
        ];

        const ext = filename.toLowerCase().substring(filename.lastIndexOf('.'));
        return mediaExtensions.includes(ext);
    }


    getFileIcon(file) {
        if (file.is_directory) return '📁';

        const ext = file.name.toLowerCase().split('.').pop();
        const iconMap = {
            // Images
            'jpg': '🖼️', 'jpeg': '🖼️', 'png': '🖼️', 'gif': '🖼️', 'svg': '🖼️', 'webp': '🖼️', 'bmp': '🖼️',
            // Videos
            'mp4': '🎬', 'avi': '🎬', 'mov': '🎬', 'mkv': '🎬', 'flv': '🎬', 'webm': '🎬', 'wmv': '🎬',
            'mpg': '🎬', 'mpeg': '🎬', '3gp': '🎬', 'ogv': '🎬', 'm4v': '🎬', 'vlc': '🎬', 'vob': '🎬',
            'ts': '🎬', 'm2ts': '🎬', 'divx': '🎬', 'xvid': '🎬', 'rm': '🎬', 'rmvb': '🎬',
            // Audio
            'mp3': '🎵', 'wav': '🎵', 'flac': '🎵', 'aac': '🎵', 'ogg': '🎵', 'wma': '🎵', 'm4a': '🎵', 'opus': '🎵',
            // Documents
            'pdf': '📄', 'doc': '📄', 'docx': '📄', 'txt': '📋', 'rtf': '📄', 'odt': '📄',
            // Spreadsheets
            'xls': '📊', 'xlsx': '📊', 'csv': '📊', 'ods': '📊',
            // Archives
            'zip': '📦', 'rar': '📦', '7z': '📦', 'tar': '📦', 'gz': '📦', 'bz2': '📦',
            // Code
            'js': '📜', 'html': '📜', 'css': '📜', 'json': '📜', 'xml': '📜', 'php': '📜',
            'cpp': '💻', 'c': '💻', 'py': '💻', 'java': '💻', 'rb': '💻', 'go': '💻',
            'rs': '💻', 'swift': '💻', 'kt': '💻', 'ts': '💻', 'jsx': '💻', 'vue': '💻',
            // Executables
            'exe': '⚙️', 'msi': '⚙️', 'app': '⚙️', 'deb': '⚙️', 'rpm': '⚙️',
            // Other
            'iso': '💿', 'img': '💿', 'dmg': '💿'
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
        this.uploadFiles(files);
    }

    handleSearch() {
        this.searchInput = document.getElementById("searchInput");
        this.clearSearchBtn = document.getElementById("clearSearchBtn");

        const query = this.searchInput.value.trim().toLowerCase();
        this.searchQuery = query;

        if (query === '') {
            this.clearSearch();
            return;
        }

        // Show clear button
        this.clearSearchBtn.classList.remove('hidden');

        // Filter files based on search query
        const filteredFiles = this.allFiles.filter(file => {
            const name = file.name.toLowerCase();
            const path = file.path.toLowerCase();
            return name.includes(query) || path.includes(query);
        });

        // Display filtered results
        this.displayFiles(filteredFiles);

        // Show search results info
        const fileList = document.getElementById('fileList');
        const resultsInfo = document.createElement('div');
        resultsInfo.className = 'search-results-info';
        resultsInfo.innerHTML = `Found <strong>${filteredFiles.length}</strong> result${filteredFiles.length !== 1 ? 's' : ''} for "<strong>${this.escapeHtml(query)}</strong>"`;

        // Insert results info before file list
        const existingInfo = fileList.querySelector('.search-results-info');
        if (existingInfo) {
            existingInfo.replaceWith(resultsInfo);
        } else if (fileList.firstChild) {
            fileList.insertBefore(resultsInfo, fileList.firstChild);
        }
    }

    clearSearch() {
        this.searchInput = document.getElementById("searchInput"); // Also need to get searchInput
        this.clearSearchBtn = document.getElementById("clearSearchBtn");

        this.searchInput.value = '';
        this.searchQuery = '';
        this.clearSearchBtn.classList.add('hidden');

        // Remove search results info
        const resultsInfo = document.querySelector('.search-results-info');
        if (resultsInfo) {
            resultsInfo.remove();
        }

        // Restore original file list
        this.displayFiles(this.allFiles);
    }
    

    async uploadFiles(files) {
        console.log('uploadFiles called with', files.length, 'files');

        const uploadProgress = document.getElementById('uploadProgress');
        const progressFill = document.getElementById('progressFill');
        const progressText = document.getElementById('progressText');

        // Show upload progress
        uploadProgress.classList.remove('hidden');

        try {
            const formData = new FormData();

            // Add each file to the form data
            Array.from(files).forEach((file, index) => {
                console.log(`Adding file ${index}:`, file.name, file.size, 'bytes');
                formData.append('files', file);
            });

            // Add current directory if we're not in root
            if (this.currentPath) {
                formData.append('directory', this.currentPath);
                console.log('Upload directory:', this.currentPath);
            }

            // Log FormData contents
            console.log('FormData entries:');
            for (let pair of formData.entries()) {
                console.log(pair[0] + ':', pair[1]);
            }

            // Create XMLHttpRequest for progress tracking
            const xhr = new XMLHttpRequest();

            // Track upload progress
            xhr.upload.addEventListener('progress', (e) => {
                if (e.lengthComputable) {
                    const percentComplete = (e.loaded / e.total) * 100;
                    progressFill.style.width = percentComplete + '%';
                    progressText.textContent = `Uploading... ${Math.round(percentComplete)}%`;
                    console.log('Upload progress:', Math.round(percentComplete) + '%');
                }
            });

            // Handle completion
            xhr.addEventListener('load', () => {
                console.log('Upload complete. Status:', xhr.status);
                console.log('Response:', xhr.responseText);

                if (xhr.status === 200) {
                    try {
                        const response = JSON.parse(xhr.responseText);
                        console.log('Parsed response:', response);

                        if (response.success) {
                            this.showToast(`Successfully uploaded ${response.uploaded.length} file(s)`, 'success');

                            // Show any errors if some files failed
                            if (response.errors && response.errors.length > 0) {
                                response.errors.forEach(error => {
                                    this.showToast(error, 'warning');
                                });
                            }

                            // Refresh the file list and stats
                            this.refresh();
                        } else {
                            this.showError('Upload failed: ' + (response.message || 'Unknown error'));
                        }
                    } catch (e) {
                        console.error('Failed to parse response:', e);
                        this.showError('Upload failed: Invalid response from server');
                    }
                } else {
                    this.showError(`Upload failed: HTTP ${xhr.status}`);
                }

                // Hide progress bar
                setTimeout(() => {
                    uploadProgress.classList.add('hidden');
                    progressFill.style.width = '0%';
                }, 1000);
            });

            // Handle errors
            xhr.addEventListener('error', (e) => {
                console.error('Upload error:', e);
                this.showError('Upload failed: Network error');
                uploadProgress.classList.add('hidden');
                progressFill.style.width = '0%';
            });

            // Log request details
            console.log('Sending POST request to /api/upload');

            // Send the request
            xhr.open('POST', '/api/upload');
            xhr.send(formData);

        } catch (error) {
            console.error('Upload error:', error);
            this.showError('Upload failed: ' + error.message);
            uploadProgress.classList.add('hidden');
            progressFill.style.width = '0%';
        }
    }

    // Add this function to your app.js temporarily for testing
    async testUploadEndpoint() {
        console.log('Testing upload endpoint...');

        // Create a simple test file
        const testContent = 'This is a test file';
        const testBlob = new Blob([testContent], { type: 'text/plain' });
        const testFile = new File([testBlob], 'test.txt', { type: 'text/plain' });

        const formData = new FormData();
        formData.append('files', testFile);

        try {
            const response = await fetch('/api/upload', {
                method: 'POST',
                body: formData
            });

            console.log('Test response status:', response.status);
            const text = await response.text();
            console.log('Test response body:', text);

            if (response.ok) {
                try {
                    const json = JSON.parse(text);
                    console.log('Parsed JSON:', json);
                } catch (e) {
                    console.log('Response is not JSON');
                }
            }
        } catch (error) {
            console.error('Test failed:', error);
        }
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