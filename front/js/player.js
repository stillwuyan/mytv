// Player wrapper for Artplayer + Hls integration
const playerModule = (function() {
    let artPlayerInstance = null;

    function detectVideoType(url) {
        if (!url) return 'unknown';
        if (url.includes('.m3u8')) return 'm3u8';
        if (url.includes('.mp4')) return 'mp4';
        if (url.includes('.webm')) return 'webm';
        return 'unknown';
    }

    function createPlayer(containerSelector, url, options = {}) {
        // destroy existing
        if (artPlayerInstance) {
            try { artPlayerInstance.destroy(); } catch (e) { console.warn(e); }
            artPlayerInstance = null;
        }

        if (!url) return null;

        if (url.includes('.m3u8') && typeof Hls === 'undefined') {
            console.error('Hls.js not loaded');
            alert('HLS播放支持加载失败，请刷新页面重试');
            return null;
        }

        artPlayerInstance = new Artplayer({
            container: containerSelector,
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
            moreVideoAttr: { crossOrigin: 'anonymous' },
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

        // double click toggle fullscreen (retain existing behavior)
        let lastClickTime = 0;
        artPlayerInstance.on('click', () => {
            const currentTime = new Date().getTime();
            if (currentTime - lastClickTime < 500) {
                artPlayerInstance.fullscreen = !artPlayerInstance.fullscreen;
            }
            lastClickTime = currentTime;
        });

        // attach optional callbacks
        if (options.onReady) options.onReady(artPlayerInstance);

        return artPlayerInstance;
    }

    function destroyPlayer() {
        if (artPlayerInstance) {
            try { artPlayerInstance.destroy(); } catch (e) { console.warn(e); }
            artPlayerInstance = null;
        }
    }

    function setDocumentTitle(title) {
        document.title = `MyTV - ${title}`;
    }

    return {
        createPlayer,
        destroyPlayer,
        setDocumentTitle,
        detectVideoType
    };
})();

window.playerModule = playerModule;
