// Views: render and manage DOM for list, tabs, and episodes
const views = (function() {
    let modalResolver = null;
    let searchStatusTimer = null;

    function escapeHtml(text) {
        const div = document.createElement('div');
        div.textContent = text;
        return div.innerHTML;
    }

    function toPlainText(html) {
        if (!html) return '';

        const div = document.createElement('div');
        div.innerHTML = html;

        return (div.textContent || div.innerText || '')
            .replace(/\u00a0/g, ' ')
            .replace(/^[\s\u3000]+/, '')
            .replace(/\s+/g, ' ')
            .trim();
    }

    function renderTitleList(data, onSelect) {
        const videoList = document.getElementById('videoList');
        videoList.innerHTML = '';

        const titles = Object.keys(data).sort();
        if (titles.length === 0) {
            videoList.innerHTML = `
                <div class="empty-state">
                    <div class="empty-state-title">当前没有可展示的影片</div>
                    <div>输入新的关键词后可刷新资源库并重新聚合结果。</div>
                </div>
            `;
            return;
        }

        titles.forEach(title => {
            const categoryItem = document.createElement('div');
            categoryItem.className = 'video-item';
            categoryItem.innerHTML = `
                <div class="video-title">${escapeHtml(title)}</div>
                <div class="video-info">包含 ${data[title].length} 个视频源</div>
                <div class="video-badge">资源已缓存</div>
            `;
            categoryItem.addEventListener('click', () => onSelect(title));
            videoList.appendChild(categoryItem);
        });
    }

    function renderSourceTabs(sources, onChange) {
        const tabsContainer = document.getElementById('sourceTabs');
        tabsContainer.innerHTML = '';
        sources.forEach((s, i) => {
            const tabButton = document.createElement('button');
            tabButton.className = 'tab-button' + (i === 0 ? ' active' : '');
            tabButton.textContent = s.source;
            tabButton.onclick = () => {
                tabsContainer.querySelectorAll('.tab-button').forEach(btn => btn.classList.remove('active'));
                tabButton.classList.add('active');
                onChange(i);
            };
            tabsContainer.appendChild(tabButton);
        });
    }

    function renderSourceDetail(source, onEpisodeSelect, activeEpisodeIndex) {
        const contentContainer = document.getElementById('sourceContent');
        contentContainer.innerHTML = '';

        const playUrls = source.play_urls || {};
        if (Object.keys(playUrls).length === 0) {
            contentContainer.innerHTML = `
                <div class="empty-state">
                    <div class="empty-state-title">当前资源没有可播放剧集</div>
                    <div>可以切换其他站点，或者重新执行搜索尝试刷新源数据。</div>
                </div>
            `;
            return;
        }

        const sourceItem = document.createElement('div');
        sourceItem.className = 'source-item';
        sourceItem.innerHTML = `
            <div class="source-header">
                <div>${escapeHtml(source.vod_name)}</div>
                <div class="video-info">${escapeHtml(source.vod_sub || '无')}</div>
                <div class="video-content">${escapeHtml(toPlainText(source.vod_content) || '暂无简介')}</div>
            </div>
        `;

        Object.entries(playUrls).forEach(([sourceName, episodes]) => {
            const sourceDiv = document.createElement('div');
            sourceDiv.innerHTML = `
                <div class="source-block-title">
                    <span>播放源: ${escapeHtml(sourceName)}</span>
                    <span class="source-block-count">${episodes.length} 集</span>
                </div>
            `;

            const gridContainer = document.createElement('div');
            gridContainer.className = 'episode-grid';

            episodes.forEach((episode, episodeIndex) => {
                const episodeItem = document.createElement('div');
                episodeItem.className = 'episode-item';
                if (activeEpisodeIndex === episodeIndex) episodeItem.classList.add('active');
                episodeItem.textContent = episode.first || episode.name || `第${episodeIndex+1}集`;
                episodeItem.setAttribute('data-url', episode.last || episode.url || '');
                episodeItem.onclick = () => onEpisodeSelect(episodeIndex, episodeItem.getAttribute('data-url'), episodeItem.textContent, gridContainer);
                gridContainer.appendChild(episodeItem);
            });

            sourceDiv.appendChild(gridContainer);
            sourceItem.appendChild(sourceDiv);
        });

        contentContainer.appendChild(sourceItem);
    }

    function showTitleListView() {
        document.getElementById('videoListContainer').classList.remove('hidden');
        document.getElementById('sourceListContainer').classList.add('hidden');
        document.getElementById('backButton').classList.add('hidden');
    }

    function showSourceView() {
        document.getElementById('videoListContainer').classList.add('hidden');
        document.getElementById('sourceListContainer').classList.remove('hidden');
        document.getElementById('backButton').classList.remove('hidden');
    }

    function getStatusDuration(tone) {
        if (tone === 'error') return 5000;
        if (tone === 'warning') return 4500;
        return 3200;
    }

    function updateStatus(message, tone, duration) {
        if (!message) {
            hideSearchStatus();
            return;
        }

        const hideAfter = duration === undefined ? getStatusDuration(tone) : duration;
        showSearchStatus(message, tone, {
            loading: false,
            duration: hideAfter
        });
    }

    function updatePlayerMeta(title, details) {
        const titleNode = document.getElementById('nowPlayingTitle');
        const metaNode = document.getElementById('nowPlayingMeta');
        if (titleNode) titleNode.textContent = title || '尚未开始播放';
        if (metaNode) metaNode.textContent = details || '支持从多个资源源聚合搜索并播放';
    }

    function togglePlayerEmpty(visible) {
        const emptyNode = document.getElementById('playerEmpty');
        if (!emptyNode) return;
        emptyNode.classList.toggle('hidden', !visible);
    }

    function showSearchStatus(message, tone, options) {
        const bar = document.getElementById('searchStatusBar');
        const text = document.getElementById('searchStatusText');
        const spinner = bar ? bar.querySelector('.search-status-spinner') : null;
        if (!bar || !text) return;

        if (searchStatusTimer) {
            window.clearTimeout(searchStatusTimer);
            searchStatusTimer = null;
        }

        const config = options || {};
        const isLoading = config.loading !== false;

        text.textContent = message || '正在搜索资源...';
        bar.className = `search-status-bar ${tone || 'info'}`;
        if (spinner) {
            spinner.classList.toggle('hidden', !isLoading);
        }
        bar.classList.remove('hidden');

        if (config.duration > 0) {
            searchStatusTimer = window.setTimeout(() => {
                hideSearchStatus();
            }, config.duration);
        }
    }

    function hideSearchStatus() {
        const bar = document.getElementById('searchStatusBar');
        if (!bar) return;

        if (searchStatusTimer) {
            window.clearTimeout(searchStatusTimer);
            searchStatusTimer = null;
        }

        bar.classList.add('hidden');
    }

    function setupModal() {
        const overlay = document.getElementById('modalOverlay');
        if (!overlay || overlay.dataset.ready === 'true') return;

        const closeButton = document.getElementById('modalClose');
        const cancelButton = document.getElementById('modalCancel');
        const confirmButton = document.getElementById('modalConfirm');

        function closeModal(result) {
            overlay.classList.add('hidden');
            overlay.setAttribute('aria-hidden', 'true');
            if (modalResolver) {
                const resolve = modalResolver;
                modalResolver = null;
                resolve(result);
            }
        }

        closeButton.addEventListener('click', () => closeModal(false));
        cancelButton.addEventListener('click', () => closeModal(false));
        confirmButton.addEventListener('click', () => closeModal(true));
        overlay.addEventListener('click', (event) => {
            if (event.target === overlay) closeModal(false);
        });
        document.addEventListener('keydown', (event) => {
            if (event.key === 'Escape' && !overlay.classList.contains('hidden')) {
                closeModal(false);
            }
        });

        overlay.dataset.ready = 'true';
    }

    function openModal(options) {
        setupModal();

        const overlay = document.getElementById('modalOverlay');
        const card = document.getElementById('modalCard');
        const icon = document.getElementById('modalIcon');
        const title = document.getElementById('modalTitle');
        const message = document.getElementById('modalMessage');
        const cancelButton = document.getElementById('modalCancel');
        const confirmButton = document.getElementById('modalConfirm');
        const closeButton = document.getElementById('modalClose');
        const tone = options.tone || 'info';
        const iconMap = {
            info: 'i',
            success: 'OK',
            warning: '!',
            error: 'X'
        };

        title.textContent = options.title || '提示';
        message.textContent = options.message || '';
        icon.textContent = iconMap[tone] || 'i';
        card.className = `modal-card ${tone}`;
        confirmButton.textContent = options.confirmText || '确定';
        cancelButton.textContent = options.cancelText || '取消';
        cancelButton.classList.toggle('hidden', !options.showCancel);
        confirmButton.classList.remove('hidden');
        cancelButton.classList.toggle('hidden', !options.showCancel);
        closeButton.classList.remove('hidden');

        overlay.classList.remove('hidden');
        overlay.setAttribute('aria-hidden', 'false');

        return new Promise((resolve) => {
            modalResolver = resolve;
            window.setTimeout(() => {
                confirmButton.focus();
            }, 0);
        });
    }

    function showConfirm(title, message, tone) {
        return openModal({
            title,
            message,
            tone: tone || 'info',
            showCancel: true,
            confirmText: '继续',
            cancelText: '取消'
        });
    }

    function showAlert(title, message, tone) {
        return openModal({
            title,
            message,
            tone: tone || 'info',
            showCancel: false,
            confirmText: '我知道了'
        });
    }

    return {
        renderTitleList,
        renderSourceTabs,
        renderSourceDetail,
        showTitleListView,
        showSourceView,
        updateStatus,
        updatePlayerMeta,
        togglePlayerEmpty,
        showSearchStatus,
        hideSearchStatus,
        showConfirm,
        showAlert
    };
})();

window.views = views;
