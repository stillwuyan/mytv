// App orchestrator: holds state and wires api, views, player together
(function() {
    const state = {
        catalog: {},
        currentTitle: null,
        currentSources: [],
        currentSourceIndex: 0,
        currentPlaylist: [],
        currentEpisodeIndex: -1,
        playerInstance: null,
        currentSourceLabel: '',
        isSearching: false,
        isUpdatingSites: false
    };

    async function refreshCatalog() {
        try {
            views.updateStatus('正在加载已缓存的影片目录...', 'info');
            const data = await api.fetchVideoCatalog();
            state.catalog = data;
            views.renderTitleList(state.catalog, openTitle);
            const titleCount = Object.keys(state.catalog).length;
            views.updateStatus(
                titleCount > 0 ? `已加载 ${titleCount} 个影片条目。` : '当前缓存目录为空，可以先执行一次搜索。',
                titleCount > 0 ? 'success' : 'warning'
            );
        } catch (err) {
            console.error('refreshCatalog error', err);
            views.updateStatus(`加载目录失败: ${err.message || err}`, 'error');
        }
    }

    function openTitle(title) {
        state.currentTitle = title;
        state.currentSources = state.catalog[title] || [];
        views.updateStatus(`已打开 ${title}，可切换不同资源站。`, 'info');
        // show view
        views.showSourceView();
        // render tabs and default content
        views.renderSourceTabs(state.currentSources, (index) => selectSource(index));
        if (state.currentSources.length > 0) selectSource(0);
    }

    function selectSource(index) {
        state.currentSourceIndex = index;
        const source = state.currentSources[index];
        if (!source) return;
        state.currentSourceLabel = source.source || '';

        // build playlist from first available play_urls group
        const playUrls = source.play_urls || {};
        // flatten to episodes array for current source (choose first key)
        const firstGroup = Object.values(playUrls)[0] || [];
        state.currentPlaylist = firstGroup.map(ep => ({ url: ep.url || ep.last || ep[1], title: ep.name || ep.first || '' }));
        state.currentEpisodeIndex = -1;

        const groupCount = Object.keys(playUrls).length;
        views.updatePlayerMeta(source.vod_name || state.currentTitle, `${source.source || '未知站点'} | ${groupCount} 个播放分组`);

        views.renderSourceDetail(source, (episodeIndex, url, title, gridContainer) => {
            // highlight logic similar to previous behavior
            // remove active from all in this grid
            gridContainer.querySelectorAll('.episode-item').forEach(item => item.classList.remove('active'));
            const clicked = gridContainer.querySelectorAll('.episode-item')[episodeIndex];
            if (clicked) clicked.classList.add('active');

            // set playlist/index
            state.currentEpisodeIndex = episodeIndex;
            // ensure playlist has entries
            if (!state.currentPlaylist[episodeIndex]) {
                // try to rebuild playlist more robustly
                state.currentPlaylist = [];
                const entries = [];
                Object.values(playUrls).forEach(group => group.forEach(ep => entries.push({ url: ep.url || ep.last || ep[1], title: ep.name || ep.first || '' })));
                state.currentPlaylist = entries;
            }

            // play
            playEpisode(episodeIndex, url, title);
        }, state.currentEpisodeIndex);
    }

    function playEpisode(idx, url, title) {
        if (!url) return;
        // destroy existing and create new
        if (state.playerInstance) {
            playerModule.destroyPlayer();
            state.playerInstance = null;
        }
        state.playerInstance = playerModule.createPlayer('.player-app', url, {});
        state.currentEpisodeIndex = idx;
        views.togglePlayerEmpty(false);
        const detectedType = playerModule.detectVideoType(url);
        views.updatePlayerMeta(
            title || state.currentTitle || '正在播放',
            `${state.currentSourceLabel || '未知站点'} | 第 ${idx + 1} 集 | ${detectedType}`
        );
        views.updateStatus(`开始播放：${title || state.currentTitle || '未命名剧集'}`, 'success');
        playerModule.setDocumentTitle(title || (state.currentTitle || ''));
    }

    function bindUIEvents() {
        document.getElementById('backButton').addEventListener('click', () => {
            views.showTitleListView();
            views.updateStatus('已返回影片目录。', 'info');
        });

        document.getElementById('searchButton').addEventListener('click', async () => {
            const keywordInput = document.getElementById('searchInput');
            const searchButton = document.getElementById('searchButton');
            const updateSitesButton = document.getElementById('updateSitesButton');
            const keyword = keywordInput.value.trim();
            if (!keyword) {
                await views.showAlert('请输入搜索关键词', '请先输入要搜索的影片、剧集或关键字。', 'warning');
                return;
            }
            const isConfirmed = await views.showConfirm('开始搜索', `确定要搜索“${keyword}”吗？\n这会刷新当前本地缓存结果。`, 'info');
            if (!isConfirmed) return;
            try {
                state.isSearching = true;
                searchButton.disabled = true;
                updateSitesButton.disabled = true;
                searchButton.textContent = '搜索中...';
                views.updateStatus(`正在搜索“${keyword}”，这会刷新本地缓存。`, 'info');
                views.showSearchStatus(`正在搜索“${keyword}”，正在刷新资源缓存...`, 'info', { loading: true });
                const res = await api.searchByKeyword(keyword);
                views.hideSearchStatus();
                views.updateStatus(res.message || '搜索成功，正在刷新目录。', 'success');
                await refreshCatalog();
                views.showSearchStatus(res.message || '资源目录已经刷新完成。', 'success', { loading: false, duration: 3200 });
            } catch (err) {
                views.hideSearchStatus();
                views.updateStatus('搜索失败: ' + (err.message || err), 'error');
                await views.showAlert('搜索失败', err.message || String(err), 'error');
            } finally {
                state.isSearching = false;
                searchButton.disabled = false;
                updateSitesButton.disabled = state.isUpdatingSites;
                searchButton.textContent = '搜索视频';
            }
        });

        document.getElementById('updateSitesButton').addEventListener('click', async () => {
            const searchButton = document.getElementById('searchButton');
            const updateSitesButton = document.getElementById('updateSitesButton');

            const isConfirmed = await views.showConfirm('更新站点', '确定要下载最新站点配置并覆盖本地文件吗？', 'info');
            if (!isConfirmed) return;

            try {
                state.isUpdatingSites = true;
                updateSitesButton.disabled = true;
                searchButton.disabled = true;
                updateSitesButton.textContent = '更新中...';
                views.updateStatus('正在更新站点配置...', 'info');
                views.showSearchStatus('正在更新站点配置并备份当前 source.json ...', 'info', { loading: true });
                const res = await api.updateSites();
                views.hideSearchStatus();
                views.updateStatus(res.message || '站点配置更新成功。', 'success');
                views.showSearchStatus(res.message || '站点配置已更新完成。', 'success', { loading: false, duration: 3200 });
            } catch (err) {
                views.hideSearchStatus();
                views.updateStatus('更新站点失败: ' + (err.message || err), 'error');
                await views.showAlert('更新站点失败', err.message || String(err), 'error');
            } finally {
                state.isUpdatingSites = false;
                updateSitesButton.disabled = false;
                searchButton.disabled = state.isSearching;
                updateSitesButton.textContent = '更新站点';
            }
        });
    }

    document.addEventListener('DOMContentLoaded', () => {
        views.togglePlayerEmpty(true);
        views.updatePlayerMeta();
        bindUIEvents();
        refreshCatalog();
    });

})();
