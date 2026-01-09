export class GithubStorage {
  constructor() {
    // We don't store state here anymore, we'll take it from the caller
    // to ensure we always use the latest UI values.
  }

  async fetchFile(token, repo, path, filename) {
    if (!token || !repo) return null;
    const trimmedToken = token.trim();
    const url = `https://api.github.com/repos/${repo}/contents/${path}/${filename}`;
    const response = await fetch(url, {
      headers: {
        Authorization: `token ${trimmedToken}`,
        Accept: 'application/vnd.github.v3.raw',
      },
    });
    if (!response.ok) {
      if (response.status === 404) return null;
      const error = new Error(`Failed to fetch file: ${response.statusText}`);
      error.status = response.status;
      throw error;
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
    const trimmedToken = token.trim();

    const baseUrl = `https://api.github.com/repos/${repo}/contents/${path}/${filename}`;

    // 1. Get the current file SHA if it exists
    let sha = null;
    const getRes = await fetch(baseUrl, {
      headers: {
        Authorization: `token ${trimmedToken}`,
      },
    });
    if (getRes.ok) {
      const data = await getRes.json();
      sha = data.sha;
    } else if (getRes.status !== 404) {
      const error = new Error(
        `Failed to check existing file: ${getRes.statusText}`
      );
      error.status = getRes.status;
      throw error;
    }

    // 2. Push the update
    const utf8Content = unescape(encodeURIComponent(content));
    const base64Content = btoa(utf8Content);

    const putRes = await fetch(baseUrl, {
      method: 'PUT',
      headers: {
        Authorization: `token ${trimmedToken}`,
        'Content-Type': 'application/json',
      },
      body: JSON.stringify({
        message: commitMessage,
        content: base64Content,
        sha: sha,
      }),
    });

    if (!putRes.ok) {
      const errData = await putRes.json();
      const error = new Error(
        `Failed to save to GitHub: ${errData.message || putRes.statusText}`
      );
      error.status = putRes.status;
      throw error;
    }
    return await putRes.json();
  }

  async listFiles(token, repo, path) {
    if (!token || !repo) {
      console.warn('listFiles: Missing token or repo', {
        token: !!token,
        repo,
      });
      return [];
    }
    const trimmedToken = token.trim();
    const url = `https://api.github.com/repos/${repo}/contents/${path}`;
    console.log(`listFiles: Fetching from GitHub: ${url}`);

    const response = await fetch(url, {
      headers: {
        Authorization: `token ${trimmedToken}`,
      },
    });

    if (!response.ok) {
      let details = '';
      let message = response.statusText;
      try {
        const errData = await response.json();
        details = JSON.stringify(errData);
        if (errData.message) message = errData.message;
      } catch (e) {
        details = await response.text();
      }
      const error = new Error(
        `GitHub API error: ${response.status} ${message}`
      );
      error.status = response.status;
      error.details = details;
      throw error;
    }

    const items = await response.json();
    console.log(`listFiles: Received ${items.length} items from GitHub`);
    console.log(
      'listFiles: Raw items:',
      items.map((i) => `${i.name} (${i.type})`).join(', ')
    );

    return items
      .filter((item) => {
        const isEligible =
          item.type === 'file' &&
          (item.name.endsWith('.js') || item.name.endsWith('.jot'));
        if (isEligible) {
          console.log(`listFiles: Found eligible file: ${item.name}`);
        }
        return isEligible;
      })
      .map((item) => item.name);
  }
}
