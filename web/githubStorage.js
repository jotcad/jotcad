export class GithubStorage {
  constructor() {
    // We don't store state here anymore, we'll take it from the caller
    // to ensure we always use the latest UI values.
  }

  async fetchFile(token, repo, path, filename) {
    if (!token || !repo) return null;
    const url = `https://api.github.com/repos/${repo}/contents/${path}/${filename}`;
    const response = await fetch(url, {
      headers: {
        Authorization: `token ${token}`,
        Accept: 'application/vnd.github.v3.raw',
      },
    });
    if (!response.ok) {
      if (response.status === 404) return null;
      throw new Error(`Failed to fetch file: ${response.statusText}`);
    }
    return await response.text();
  }

  async saveFile(
    token,
    repo,
    path,
    filename,
    content,
    commitMessage = 'Update design'
  ) {
    if (!token || !repo) throw new Error('GitHub credentials not set');

    const baseUrl = `https://api.github.com/repos/${repo}/contents/${path}/${filename}`;

    // 1. Get the current file SHA if it exists
    let sha = null;
    const getRes = await fetch(baseUrl, {
      headers: {
        Authorization: `token ${token}`,
      },
    });
    if (getRes.ok) {
      const data = await getRes.json();
      sha = data.sha;
    }

    // 2. Push the update
    const utf8Content = unescape(encodeURIComponent(content));
    const base64Content = btoa(utf8Content);

    const putRes = await fetch(baseUrl, {
      method: 'PUT',
      headers: {
        Authorization: `token ${token}`,
        'Content-Type': 'application/json',
      },
      body: JSON.stringify({
        message: commitMessage,
        content: base64Content,
        sha: sha,
      }),
    });

    if (!putRes.ok) {
      const err = await putRes.json();
      throw new Error(`Failed to save to GitHub: ${err.message}`);
    }
    return await putRes.json();
  }

  async listFiles(token, repo, path) {
    if (!token || !repo) return [];
    const url = `https://api.github.com/repos/${repo}/contents/${path}`;
    const response = await fetch(url, {
      headers: {
        Authorization: `token ${token}`,
      },
    });
    if (!response.ok) return [];
    const items = await response.json();
    return items
      .filter((item) => item.type === 'file' && item.name.endsWith('.js'))
      .map((item) => item.name);
  }
}
