//! The native side of the TON618 IDE: files on disk and running the real
//! `ton618` interpreter (bundled next to the app as a "sidecar" binary).
//! Everything else — the editor, the built-in engine — is in ../web.

use std::collections::HashMap;
use std::io::{Read, Write};
use std::path::{Component, Path, PathBuf};
use std::process::{Child, ChildStdin, Command, Stdio};
use std::sync::{Arc, Mutex};
use std::time::Duration;

use tauri::{AppHandle, Emitter, State};

#[derive(Default)]
struct Running {
    child: Arc<Mutex<Option<Child>>>,
    stdin: Mutex<Option<ChildStdin>>,
}

#[derive(Clone, serde::Serialize)]
struct Output {
    stream: &'static str,
    text: String,
}

#[derive(Clone, serde::Serialize)]
struct Exit {
    code: i32,
}

#[tauri::command]
fn platform() -> &'static str {
    std::env::consts::OS
}

const TEXT_EXTENSIONS: &[&str] = &["ton", "txt", "md", "json", "html", "css", "js", "csv"];

/// The editable files of a folder, as "/"-separated paths relative to it.
#[tauri::command]
fn list_files(dir: String) -> Result<Vec<String>, String> {
    fn walk(root: &Path, dir: &Path, out: &mut Vec<String>, depth: usize) {
        if depth > 6 || out.len() > 2000 {
            return;
        }
        let Ok(entries) = std::fs::read_dir(dir) else { return };
        for entry in entries.flatten() {
            let path = entry.path();
            let name = entry.file_name().to_string_lossy().to_string();
            if name.starts_with('.') || name == "node_modules" || name == "target" {
                continue;
            }
            if path.is_dir() {
                walk(root, &path, out, depth + 1);
            } else if path
                .extension()
                .map(|e| TEXT_EXTENSIONS.contains(&e.to_string_lossy().to_lowercase().as_str()))
                .unwrap_or(false)
            {
                if let Ok(rel) = path.strip_prefix(root) {
                    out.push(rel.components().map(|c| c.as_os_str().to_string_lossy()).collect::<Vec<_>>().join("/"));
                }
            }
        }
    }
    let root = PathBuf::from(&dir);
    if !root.is_dir() {
        return Err(format!("{dir} is not a folder."));
    }
    let mut out = Vec::new();
    walk(&root, &root, &mut out, 0);
    out.sort();
    Ok(out)
}

#[tauri::command]
fn read_text(path: String) -> Result<String, String> {
    std::fs::read_to_string(&path).map_err(|e| format!("Cannot read {path}: {e}"))
}

#[tauri::command]
fn write_text(path: String, content: String) -> Result<(), String> {
    if let Some(parent) = Path::new(&path).parent() {
        std::fs::create_dir_all(parent).map_err(|e| e.to_string())?;
    }
    std::fs::write(&path, content).map_err(|e| format!("Cannot save {path}: {e}"))
}

/// The bundled interpreter (next to the IDE's own executable), or `ton618`
/// from the PATH when it isn't there (e.g. during development).
fn interpreter() -> PathBuf {
    let name = format!("ton618{}", std::env::consts::EXE_SUFFIX);
    if let Ok(exe) = std::env::current_exe() {
        if let Some(dir) = exe.parent() {
            let bundled = dir.join(&name);
            if bundled.is_file() {
                return bundled;
            }
        }
    }
    PathBuf::from(name)
}

fn spawn(app: AppHandle, running: &Running, dir: &Path, file: &str) -> Result<(), String> {
    stop(running);
    let mut cmd = Command::new(interpreter());
    cmd.arg(file)
        .current_dir(dir)
        .env("TON618_UNBUFFERED", "1")
        .stdin(Stdio::piped())
        .stdout(Stdio::piped())
        .stderr(Stdio::piped());
    #[cfg(windows)]
    {
        use std::os::windows::process::CommandExt;
        cmd.creation_flags(0x0800_0000); // CREATE_NO_WINDOW: no console window flashing
    }
    let mut child = cmd
        .spawn()
        .map_err(|e| format!("Could not start the ton618 interpreter ({e}). Install it, or use the built-in engine."))?;

    let mut readers = Vec::new();
    for (stream, pipe) in [
        ("stdout", child.stdout.take().map(|p| Box::new(p) as Box<dyn Read + Send>)),
        ("stderr", child.stderr.take().map(|p| Box::new(p) as Box<dyn Read + Send>)),
    ] {
        let Some(mut pipe) = pipe else { continue };
        let app = app.clone();
        readers.push(std::thread::spawn(move || {
            let mut buf = [0u8; 4096];
            while let Ok(n) = pipe.read(&mut buf) {
                if n == 0 {
                    break;
                }
                let text = String::from_utf8_lossy(&buf[..n]).to_string();
                let _ = app.emit("run-output", Output { stream, text });
            }
        }));
    }
    *running.stdin.lock().unwrap() = child.stdin.take();
    *running.child.lock().unwrap() = Some(child);

    // Waits for the program to end (or be stopped), then reports its exit code.
    let child = running.child.clone();
    std::thread::spawn(move || {
        let code = loop {
            let mut guard = child.lock().unwrap();
            match guard.as_mut() {
                None => break -1, // stopped
                Some(c) => match c.try_wait() {
                    Ok(Some(status)) => {
                        *guard = None;
                        break status.code().unwrap_or(-1);
                    }
                    Ok(None) => {}
                    Err(_) => break -1,
                },
            }
            drop(guard);
            std::thread::sleep(Duration::from_millis(30));
        };
        for r in readers {
            let _ = r.join();
        }
        let _ = app.emit("run-exit", Exit { code });
    });
    Ok(())
}

fn stop(running: &Running) {
    *running.stdin.lock().unwrap() = None;
    if let Some(mut child) = running.child.lock().unwrap().take() {
        let _ = child.kill();
        let _ = child.wait();
    }
}

/// Runs `file` (relative to `dir`) from a folder on disk.
#[tauri::command]
fn run_native(app: AppHandle, running: State<Running>, dir: String, file: String) -> Result<(), String> {
    spawn(app, &running, Path::new(&dir), &file)
}

/// Runs a workspace that only exists in the app: its files are written to a
/// temporary folder first, so IMPORT:// between them works.
#[tauri::command]
fn run_native_files(
    app: AppHandle,
    running: State<Running>,
    files: HashMap<String, String>,
    file: String,
) -> Result<(), String> {
    let dir = std::env::temp_dir().join(format!("ton618-ide-{}", std::process::id()));
    let _ = std::fs::remove_dir_all(&dir);
    for (name, content) in &files {
        let rel = Path::new(name);
        if rel.components().any(|c| !matches!(c, Component::Normal(_))) {
            continue; // never write outside the temporary folder
        }
        let path = dir.join(rel);
        if let Some(parent) = path.parent() {
            std::fs::create_dir_all(parent).map_err(|e| e.to_string())?;
        }
        std::fs::write(&path, content).map_err(|e| e.to_string())?;
    }
    spawn(app, &running, &dir, &file)
}

/// A line typed in the IDE's input box, for the program's input() calls.
#[tauri::command]
fn send_input(running: State<Running>, text: String) -> Result<(), String> {
    let mut guard = running.stdin.lock().unwrap();
    let stdin = guard.as_mut().ok_or("The program is not running.")?;
    stdin.write_all(format!("{text}\n").as_bytes()).and_then(|_| stdin.flush()).map_err(|e| e.to_string())
}

#[tauri::command]
fn stop_run(running: State<Running>) {
    stop(&running);
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tauri::Builder::default()
        .plugin(tauri_plugin_dialog::init())
        .manage(Running::default())
        .invoke_handler(tauri::generate_handler![
            platform,
            list_files,
            read_text,
            write_text,
            run_native,
            run_native_files,
            send_input,
            stop_run
        ])
        .run(tauri::generate_context!())
        .expect("error while running the TON618 IDE");
}
