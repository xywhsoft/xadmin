"""Functional updater tests in a disposable xadmin site. No public publishing."""
import argparse
import hashlib
import json
import socket
import subprocess
import time
from pathlib import Path

import smoke


def multipart(platform, payload, filename, notes=""):
    boundary = "mdo-update-test-boundary"
    fields = []
    for name, value in (("platform", platform), ("notes", notes)):
        fields.append(("--" + boundary + '\r\nContent-Disposition: form-data; name="' +
                       name + '"\r\n\r\n' + value + "\r\n").encode())
    fields.append(("--" + boundary + '\r\nContent-Disposition: form-data; name="file"; filename="' +
                   filename + '"\r\nContent-Type: application/octet-stream\r\n\r\n').encode())
    return b"".join(fields) + payload + ("\r\n--" + boundary + "--\r\n").encode(), {
        "Content-Type": "multipart/form-data; boundary=" + boundary}


def run(exe, apk):
    with socket.socket() as listener:
        listener.bind(("127.0.0.1", 0))
        port = listener.getsockname()[1]
    target = smoke.fixture(port)
    config_path = target / "xs.json"
    config = json.loads(config_path.read_text())
    config["services"][0]["recv_limit"] = 33 * 1024 * 1024
    config["services"][0]["body_limit"] = 32 * 1024 * 1024 + 8192
    config_path.write_text(json.dumps(config), encoding="utf-8")
    log = open(target / "update-test.log", "wb")
    proc = subprocess.Popen([str(smoke.DEFAULT_EXECUTABLE), str(target / "xs.json")],
                            cwd=smoke.ROOT, stdout=log, stderr=log,
                            creationflags=subprocess.CREATE_NO_WINDOW)
    count = 0

    def check(value, detail):
        nonlocal count
        assert value, detail
        count += 1
        print("PASS", detail)

    def request(method, path, data=None, cookie=None, headers=None):
        return smoke.request(port, method, path, data, cookie, headers)

    try:
        for _ in range(100):
            if proc.poll() is not None:
                raise RuntimeError("fixture host exited")
            try:
                if request("GET", "/admin/login")[0] == 200:
                    break
            except OSError:
                pass
            time.sleep(.2)
        st, hd, body = request("POST", "/admin/login", {
            "username": smoke.USER, "password": smoke.client_hash(smoke.USER, smoke.PASSWORD)})
        check(st == 200 and json.loads(body)["result"], "fixture login")
        cookie = hd["Set-Cookie"].split(";")[0]
        st, _, body = request("POST", "/admin/plugin/enable", {"name": "mdo-update"}, cookie)
        check(st == 200 and json.loads(body)["result"], "TCC plugin enable: " + body.decode())
        check(request("GET", "/update/version")[0] == 404, "empty package is 404")
        check(request("GET", "/admin/api/mdo-update")[0] == 302, "admin login required")
        st, _, body = request("GET", "/admin/api/mdo-update", cookie=cookie)
        inventory = json.loads(body)["data"]
        csrf = inventory["csrf_token"]
        check(st == 200 and inventory["max_size"] == 32*1024*1024, "inventory and CSRF")
        denied = smoke.USER + "_denied"
        _, hd, _ = request("POST", "/admin/login", {
            "username": denied, "password": smoke.client_hash(denied, smoke.PASSWORD)})
        check(request("GET", "/admin/api/mdo-update", cookie=hd["Set-Cookie"].split(";")[0])[0] == 403,
              "admin without upload permission is denied")
        for platform, source in (("windows-x86_64", exe), ("android-arm64-v8a", apk)):
            content = source.read_bytes()
            form, headers = multipart(platform, content, source.name, "测试更新")
            check(request("POST", "/admin/api/mdo-update/upload", form, cookie, headers)[0] == 403,
                  platform + " rejects missing CSRF")
            headers["X-CSRF-Token"] = csrf
            st, _, body = request("POST", "/admin/api/mdo-update/upload", form, cookie, headers)
            check(st == 200, platform + " upload: " + body[:200].decode())
            info = json.loads(body)
            check(info["sha256"] == hashlib.sha256(content).hexdigest() and info["size"] == len(content),
                  platform + " hashes saved bytes")
            st, hd, downloaded = request("GET", info["url"])
            check(st == 200 and downloaded == content, platform + " exact public download: " + str(st))
            check("immutable" in hd.get("Cache-Control", ""), platform + " immutable URL")
            st, _, body = request("HEAD", info["url"])
            check(st == 200 and not body, platform + " HEAD")
            check(request("GET", info["url"] + "a")[0] == 404, "oversize hash rejected")
            st, _, body = request("GET", "/update/version?platform=" + platform)
            check(st == 200 and json.loads(body) == info, platform + " public metadata")
            before = (target / "plugin_data/mdo-update/current.json").read_bytes()
            st, _, _ = request("POST", "/admin/api/mdo-update/upload", form, cookie, headers)
            check(st == 200 and before == (target / "plugin_data/mdo-update/current.json").read_bytes(),
                  platform + " identical upload does not change publication")
            invalid, _ = multipart(platform, b"invalid package", source.name)
            check(request("POST", "/admin/api/mdo-update/upload", invalid, cookie, headers)[0] == 422,
                  platform + " invalid format rejected")
            check(request("POST", "/admin/api/mdo-update/upload", form[:-8], cookie, headers)[0] == 422,
                  platform + " truncated multipart rejected")
            check(before == (target / "plugin_data/mdo-update/current.json").read_bytes(),
                  platform + " failed upload preserves publication")
        check(not (target / "plugin_data/mdo-update/packages/upload.part").exists(), "staging file removed")
        check(request("GET", "/update/version?platform=linux")[0] == 400, "unsupported platform")
        st, _, body = request("POST", "/admin/plugin/reload", {"name": "mdo-update"}, cookie)
        check(st == 200 and json.loads(body)["result"], "plugin reload")
        check(request("GET", "/update/version")[0] == 200, "metadata survives reload")
        # Atomic metadata failure must not advance the public pointer.
        saved = target / "plugin_data/mdo-update/current.json"
        saved.rename(saved.with_suffix(".test-backup"))
        saved.mkdir()
        altered = bytearray(exe.read_bytes())
        altered[2] ^= 1  # DOS padding; still a structurally valid packed PE.
        form, headers = multipart("windows-x86_64", bytes(altered), "mdo.exe")
        headers["X-CSRF-Token"] = csrf
        st, _, _ = request("POST", "/admin/api/mdo-update/upload", form, cookie, headers)
        check(st == 503, "metadata commit failure is reported")
        st, _, info = request("GET", "/update/version")
        check(json.loads(info)["sha256"] == hashlib.sha256(exe.read_bytes()).hexdigest(),
              "metadata failure leaves current package downloadable")
        saved.rmdir()
        saved.with_suffix(".test-backup").rename(saved)
        old = json.loads(info)
        st, _, body = request("POST", "/admin/api/mdo-update/upload", form, cookie, headers)
        check(st == 200, "publish second Windows package")
        second = json.loads(body)
        check(request("GET", old["url"])[2] == exe.read_bytes(), "previous package remains available")
        altered[3] ^= 1
        form, headers = multipart("windows-x86_64", bytes(altered), "mdo.exe")
        headers["X-CSRF-Token"] = csrf
        check(request("POST", "/admin/api/mdo-update/upload", form, cookie, headers)[0] == 200,
              "publish third Windows package")
        check(request("GET", old["url"])[0] == 404 and
              not (target / ("plugin_data/mdo-update/packages/" + old["sha256"] + ".exe")).exists(),
              "obsolete package and URL removed")
        check(request("GET", second["url"])[0] == 200, "one previous package retained")
        object_path = target / ("plugin_data/mdo-update/packages/" + second["sha256"] + ".exe")
        object_path.write_bytes(b"damaged")
        check(request("GET", second["url"])[0] == 503, "damaged object is never served")
        print(f"{count} updater checks passed; fixture={target}")
    except BaseException:
        log.flush()
        print((target / "update-test.log").read_text(errors="replace")[-5000:])
        raise
    finally:
        proc.terminate()
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait(timeout=5)
        log.close()


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--exe", type=Path, default=Path("D:/GIT/mdo/mdo.exe"))
    parser.add_argument("--apk", type=Path, default=Path("D:/GIT/mdo/mdo-arm64-v8a.apk"))
    args = parser.parse_args()
    run(args.exe, args.apk)
