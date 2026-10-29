# AUR packaging

This directory holds the Arch User Repository package for angsh
(`https://aur.archlinux.org/packages/angsh`). The AUR repo is a separate
git remote — these files are the reviewed source of truth kept in-tree.

## First-time submission

1. Create an account at https://aur.archlinux.org/register/ (or log in).
2. Add the machine's SSH public key under **My Account** → *SSH Public Key*.
3. Push:

   ```sh
   git clone ssh://aur@aur.archlinux.org/angsh.git /tmp/angsh-aur
   cp PKGBUILD angsh.install .SRCINFO /tmp/angsh-aur/
   cd /tmp/angsh-aur
   git add PKGBUILD angsh.install .SRCINFO
   git commit -m "angsh 0.1.1-1"
   git push origin master
   ```

## On each new release

1. Bump `pkgver` in `PKGBUILD` (and reset `pkgrel=1`).
2. Refresh the checksum:

   ```sh
   curl -fsSLO https://github.com/0x251C08/ang.sh/archive/refs/tags/v<ver>.tar.gz
   mv v<ver>.tar.gz angsh-<ver>.tar.gz && sha256sum angsh-<ver>.tar.gz
   ```

3. Regenerate metadata (mandatory — the AUR rejects pushes without a fresh
   `.SRCINFO`):

   ```sh
   makepkg --printsrcinfo > .SRCINFO
   ```

4. Test locally: `makepkg -f`, then `makepkg -si` to install.
5. Push to the AUR remote as above (commit message: `angsh <ver>-<rel>`).
