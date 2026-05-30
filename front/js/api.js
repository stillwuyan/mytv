// Lightweight API layer - centralized endpoints and normalization
const api = (function() {
    const PATHS = {
        videos: '/api/videos',
        search: '/api/search',
        update: '/api/update'
    };

    async function fetchVideoCatalog() {
        try {
            const res = await fetch(PATHS.videos);
            if (!res.ok) throw new Error('Failed to fetch videos: ' + res.status);
            return await res.json();
        } catch (err) {
            console.error(err);
            throw err;
        }
    }

    async function searchByKeyword(keyword) {
        try {
            const res = await fetch(PATHS.search, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ keyword })
            });
            // Expect JSON response (backend updated to return JSON)
            const data = await res.json();
            if (!res.ok) throw new Error(data.message || 'Search failed');
            return data;
        } catch (err) {
            console.error('searchByKeyword error', err);
            throw err;
        }
    }

    async function updateSites() {
        try {
            const res = await fetch(PATHS.update, {
                method: 'POST'
            });
            const data = await res.json();
            if (!res.ok) throw new Error(data.message || 'Update failed');
            return data;
        } catch (err) {
            console.error('updateSites error', err);
            throw err;
        }
    }

    return {
        fetchVideoCatalog,
        searchByKeyword,
        updateSites
    };
})();

// expose to global for legacy compatibility
window.api = api;
