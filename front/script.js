let artPlayer = null;
let currentVideo = null;
let currentCategory = null;
let videoData = {};

// 添加搜索功能
async function searchVideos() {
    // 从输入框获取关键词，而不是使用prompt
    const keywordInput = document.getElementById('searchInput');
    const keyword = keywordInput.value.trim();

    if (!keyword) {
        alert('请输入搜索关键词');
        return;
    }

    // 添加确认弹窗
    const isConfirmed = confirm(`确定要搜索"${keyword}"吗？`);
    if (!isConfirmed) {
        console.log('用户取消了搜索操作');
        return; // 用户点击取消，中断搜索
    }

    try {
        const response = await fetch('/api/search', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify({ keyword: keyword })
        });

        if (response.ok) {
            alert('搜索成功！页面将刷新显示新结果');
            // 重新加载视频数据
            loadVideoData();
        } else {
            alert('搜索失败: ' + await response.text());
        }
    } catch (error) {
        console.error('搜索请求失败:', error);
        alert('搜索请求失败，请检查网络连接');
    }
}

// 检测视频类型
function detectVideoType(url) {
    if (url.includes('.m3u8')) return 'm3u8';
    if (url.includes('.mp4')) return 'mp4';
    if (url.includes('.webm')) return 'webm';
    return 'unknown';
}

// 获取视频数据
async function loadVideoData() {
    try {
        const response = await fetch('/api/videos');
        const data = await response.json();
        videoData = data;
        displayCategoryList(data);
    } catch (error) {
        console.error('获取视频数据失败:', error);
    }
}

// 显示分类列表（videoData的key）
function displayCategoryList(data) {
    const videoList = document.getElementById('videoList');
    videoList.innerHTML = '';

    const categories = Object.keys(data);
    categories.sort();

    categories.forEach(category => {
        const categoryItem = document.createElement('div');
        categoryItem.className = 'video-item';
        categoryItem.innerHTML = `
            <div class="video-title">${escapeHtml(category)}</div>
            <div class="video-info">包含 ${data[category].length} 个视频源</div>
        `;

        categoryItem.addEventListener('click', () => {
            showSourceList(category, data[category]);
        });

        videoList.appendChild(categoryItem);
    });
}

// 显示视频源列表
function showSourceList(category, sources) {
    currentCategory = category;

    // 隐藏分类列表，显示源列表
    document.getElementById('videoListContainer').classList.add('hidden');
    document.getElementById('sourceListContainer').classList.remove('hidden');
    document.getElementById('backButton').classList.remove('hidden');

    const tabsContainer = document.getElementById('sourceTabs');
    const contentContainer = document.getElementById('sourceContent');

    tabsContainer.innerHTML = '';
    contentContainer.innerHTML = '';

    // 创建标签页
    sources.forEach((source, index) => {
        const tabButton = document.createElement('button');
        tabButton.className = 'tab-button' + (index === 0 ? ' active' : '');
        tabButton.textContent = source.source;
        tabButton.onclick = () => {
            // 移除其他标签的active类
            tabsContainer.querySelectorAll('.tab-button').forEach(btn => {
                btn.classList.remove('active');
            });
            tabButton.classList.add('active');
            showSourceContent(source, contentContainer);
        };
        tabsContainer.appendChild(tabButton);
    });

    // 默认显示第一个源的内容
    if (sources.length > 0) {
        showSourceContent(sources[0], contentContainer);
    }
}

// 显示单个视频源的详细内容
function showSourceContent(source, container) {
    container.innerHTML = '';

    const sourceItem = document.createElement('div');
    sourceItem.className = 'source-item';
    sourceItem.innerHTML = `
        <div class="source-header">
            <div>${escapeHtml(source.vod_name)}</div>
            <div class="video-info">${source.vod_sub || '无'}</div>
            <div class="video-content">${source.vod_content || '暂无简介'}</div>
        </div>
    `;

    // 显示播放源和剧集
    const playUrls = source.play_urls || {};
    Object.entries(playUrls).forEach(([sourceName, episodes]) => {
        const sourceDiv = document.createElement('div');
        sourceDiv.innerHTML = `
            <div class="video-info" style="margin-top: 15px; font-weight: bold;">
                播放源: ${escapeHtml(sourceName)}
            </div>
        `;

        const gridContainer = document.createElement('div');
        gridContainer.className = 'episode-grid';

        episodes.forEach((episode, episodeIndex) => {
            const episodeItem = document.createElement('div');
            episodeItem.className = 'episode-item';
            episodeItem.textContent = episode.name;
            episodeItem.setAttribute('data-url', episode.url);
            episodeItem.setAttribute('data-title', episode.name);
            episodeItem.onclick = () => {
                // 移除其他剧集的高亮
                const episodeGrids = document.querySelectorAll('.episode-grid');
                episodeGrids.forEach(grid => {
                    grid.querySelectorAll('.episode-item').forEach(item => {
                        item.classList.remove('active');
                    });
                });

                // 添加当前剧集的高亮
                episodeItem.classList.add('active');

                updatePageTitle(episode.name);

                // 设置当前播放列表和索引
                currentPlaylist = episodes.map((ep, idx) => ({
                    url: ep.url,
                    title: ep.name || `第${idx + 1}集`
                }));
                currentIndex = episodeIndex;

                initArtPlayer(episode.url, episode.name);
            };
            gridContainer.appendChild(episodeItem);
        });

        sourceDiv.appendChild(gridContainer);
        sourceItem.appendChild(sourceDiv);
    });

    container.appendChild(sourceItem);
}

// 返回分类列表
function backToCategoryList() {
    document.getElementById('videoListContainer').classList.remove('hidden');
    document.getElementById('sourceListContainer').classList.add('hidden');
    document.getElementById('backButton').classList.add('hidden');
}

// HTML转义
function escapeHtml(text) {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
}

// 页面加载完成后获取数据
document.addEventListener('DOMContentLoaded', () => {
    loadVideoData();
    document.getElementById('backButton').addEventListener('click', backToCategoryList);
    // 添加搜索按钮事件监听
    document.getElementById('searchButton').addEventListener('click', searchVideos);
});

// 初始化ArtPlayer播放器
function initArtPlayer(url, playlist = [], currentIndex = 0) {
    // 检查Hls.js是否已加载（对于m3u8格式）
    if (url.includes('.m3u8') && typeof Hls === 'undefined') {
        console.error('Hls.js未正确加载，请检查CDN链接');
        alert('HLS播放支持加载失败，请刷新页面重试');
        return;
    }

    // 销毁现有的播放器
    if (artPlayer) {
        artPlayer.destroy();
        artPlayer = null;
    }

    // 创建新的ArtPlayer实例
    artPlayer = new Artplayer({
        container: '.artplayer-app',
        url: url,
        type: detectVideoType(url),
        autoplay: true,
        volume: 0.7,
        isLive: false,
        muted: false,
        autoOrientation: true,
        pip: true,
        autoSize: true,
        fastForward: true,
        autoPlayback: true,
        fullscreen: true,
        fullscreenWeb: false,
        setting: false,
        hotkey: true,
        moreVideoAttr: {
            crossOrigin: 'anonymous'
        },
        // 添加上一集和下一集控制按钮
        controls: [
            {
                position: 'right',
                html: '<svg width="24" height="24" viewBox="0 0 24 24" fill="none" xmlns="http://www.w3.org/2000/svg"><path d="M6 6h2v12H6zm3.5 6l8.5 6V6z" fill="currentColor"/></svg>',
                tooltip: '上一集',
                click: function() {
                    playPrevious();
                }
            },
            {
                position: 'right',
                html: '<svg width="24" height="24" viewBox="0 0 24 24" fill="currentColor"><path d="M18 6v12h-2V6h2zm-3.5 6l-8.5 6V6z"/></svg>',
                tooltip: '下一集',
                click: function() {
                    playNext();
                }
            }
        ],
        customType: {
            m3u8: function(video, url, art) {
                if (Hls && Hls.isSupported()) {
                    const hls = new Hls();
                    hls.loadSource(url);
                    hls.attachMedia(video);
                    art.on('destroy', () => hls.destroy());
                } else if (video.canPlayType('application/vnd.apple.mpegurl')) {
                    video.src = url;
                } else {
                    art.notice.show('不支持播放该视频格式');
                }
            }
        }
    });
}

// 播放上一集
function playPrevious() {
    if (currentPlaylist.length === 0 || currentIndex <= 0) return;

    const prevIndex = currentIndex - 1;
    const prevEpisode = currentPlaylist[prevIndex];

    if (prevEpisode && prevEpisode.url) {
        // 更新显示
        updateEpisodeHighlight(currentIndex, prevIndex);
        updatePageTitle(prevEpisode.title);

        currentIndex = prevIndex;
        initArtPlayer(prevEpisode.url, prevEpisode.title);
    }
}

function playNext() {
    if (currentPlaylist.length === 0 || currentIndex >= currentPlaylist.length - 1) return;

    const nextIndex = currentIndex + 1;
    const nextEpisode = currentPlaylist[nextIndex];

    if (nextEpisode && nextEpisode.url) {
        // 更新显示
        updateEpisodeHighlight(currentIndex, nextIndex);
        updatePageTitle(nextEpisode.title);

        currentIndex = nextIndex;
        initArtPlayer(nextEpisode.url, nextEpisode.title);
    }
}

// 更新剧集高亮显示
function updateEpisodeHighlight(currentIdx, nextIdx) {
    // 查找所有剧集网格容器
    const episodeGrids = document.querySelectorAll('.episode-grid');

    episodeGrids.forEach(grid => {
        const episodeItems = grid.querySelectorAll('.episode-item');
        if (episodeItems[currentIdx] && episodeItems[currentIdx].classList.contains('active')) {
            // 移除所有高亮
            grid.querySelectorAll('.episode-item').forEach(item => {
                item.classList.remove('active');
            });

            // 为当前索引的剧集添加高亮
            if (episodeItems[nextIdx]) {
                episodeItems[nextIdx].classList.add('active');
            }
        }
    });
}

function updatePageTitle(episodeTitle) {
    document.title = `MyTV - ${episodeTitle}`;
}
