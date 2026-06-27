const vscode = require('vscode');
const { SerialPort } = require('serialport');

class SerialPty {
  constructor(portPath, baud) {
    this.portPath = portPath;
    this.baud = baud;
    this.writeEmitter = new vscode.EventEmitter();
    this.onDidWrite = this.writeEmitter.event;
    this.closeEmitter = new vscode.EventEmitter();
    this.onDidClose = this.closeEmitter.event;
    this.ready = null;
    this.disposed = false;
    this.reconnecting = false;
    this.reconnectDelay = 1000;
  }

  open() {
    if (this.ready) return this.ready;
    this.ready = this._connect();
    return this.ready;
  }

  async _connect() {
    this.port = new SerialPort({ path: this.portPath, baudRate: this.baud, autoOpen: false });
    await new Promise((resolve, reject) => {
      this.port.open((err) => (err ? reject(err) : resolve()));
    });
    this.writeEmitter.fire(`Connected to ${this.portPath} @ ${this.baud}\r\n`);
    this.port.on('data', (data) => {
      this.writeEmitter.fire(data.toString());
    });
    this.port.on('error', (e) => {
      this.writeEmitter.fire(`Serial error: ${e.message}\r\n`);
    });
    this.port.on('close', () => {
      this.writeEmitter.fire(`Port closed, retrying...\r\n`);
      this.port = null;
      this.ready = null;
      if (!this.disposed) {
        this._scheduleReconnect();
      } else {
        this.closeEmitter.fire();
      }
    });
  }

  isOpen() {
    return !!(this.port && this.port.isOpen);
  }

  close() {
    this.disposed = true;
    if (this.port && this.port.isOpen) {
      this.port.close();
    }
    this.closeEmitter.fire();
  }

  _scheduleReconnect() {
    if (this.reconnecting) return;
    this.reconnecting = true;
    setTimeout(async () => {
      this.reconnecting = false;
      if (this.disposed) return;
      try {
        await this.open();
      } catch (err) {
        this.writeEmitter.fire(`Reconnect failed: ${err.message}\r\n`);
        this._scheduleReconnect();
      }
    }, this.reconnectDelay);
  }

  handleInput(data) {
    if (this.port && this.port.isOpen) {
      this.port.write(data, () => {});
    }
  }
}

function delay(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

async function pickPort() {
  const ports = await SerialPort.list();
  const items = ports.map((p) => ({
    label: p.path,
    description: `${p.friendlyName || ''} ${p.manufacturer || ''}`.trim(),
    path: p.path,
  }));
  items.push({ label: 'Enter path…', description: 'Manual entry (e.g. COM3)', path: null });
  const choice = await vscode.window.showQuickPick(items, {
    placeHolder: 'Select PicoMite serial port',
    ignoreFocusOut: true,
  });
  if (!choice) return undefined;
  if (choice.path) return choice.path;
  const manual = await vscode.window.showInputBox({
    prompt: 'Enter serial port path (e.g. COM3)',
    ignoreFocusOut: true,
  });
  return manual || undefined;
}

async function sendToPicoMite() {
  const editor = vscode.window.activeTextEditor;
  if (!editor) {
    vscode.window.showErrorMessage('No active editor');
    return;
  }
  const doc = editor.document;
  const text = doc.getText();
  const lines = text.split(/\r?\n/);
  const cfg = vscode.workspace.getConfiguration();
  const baud = cfg.get('mmbasicUploader.baud', 115200);
  const lineDelay = cfg.get('mmbasicUploader.lineDelayMs', 5);

  const portPath = await pickPort();
  if (!portPath) return;

  const channel = vscode.window.createOutputChannel('MMBasic Upload');
  channel.show(true);

  let port;
  let reuse = false;
  if (currentPty && currentPty.portPath === portPath) {
    await currentPty.open().catch(() => {});
    if (currentPty.isOpen()) {
      port = currentPty.port;
      reuse = true;
      channel.appendLine(`Using open console port ${portPath} @ ${baud} baud...`);
    }
  }

  if (!port) {
    channel.appendLine(`Connecting to ${portPath} @ ${baud} baud...`);
    try {
      port = new SerialPort({ path: portPath, baudRate: baud, autoOpen: false });
      await new Promise((res, rej) => port.open((err) => (err ? rej(err) : res())));
    } catch (err) {
      vscode.window.showErrorMessage(`Failed to open port: ${err.message || err}`);
      channel.appendLine(`Error: ${err.stack || err}`);
      return;
    }
  }

  const writeChunk = (buf) =>
    new Promise((res, rej) => {
      port.write(buf, (err) => (err ? rej(err) : res()));
    });

  try {
    await writeChunk(Buffer.from('AUTOSAVE\r\n', 'utf8'));
    for (const line of lines) {
      await delay(lineDelay);
      await writeChunk(Buffer.from(line + '\r\n', 'utf8'));
    }
    await delay(lineDelay);
    await writeChunk(Buffer.from([0x1a]));
    channel.appendLine('Upload complete (sent CTRL+Z)');
  } catch (err) {
    vscode.window.showErrorMessage(`Upload failed: ${err.message || err}`);
    channel.appendLine(`Error: ${err.stack || err}`);
  } finally {
    await new Promise((res) => port.drain(() => res()));
    if (!reuse) port.close();
  }
}

async function openConsole() {
  const cfg = vscode.workspace.getConfiguration();
  const baud = cfg.get('mmbasicUploader.baud', 115200);
  const portPath = await pickPort();
  if (!portPath) return;
  const pty = new SerialPty(portPath, baud);
  currentPty = pty;
  const term = vscode.window.createTerminal({ name: `MMBasic Console (${portPath})`, pty });
  term.show(true);
}

function activate(context) {
  const disposable = vscode.commands.registerCommand('mmbasic.sendToPicoMite', sendToPicoMite);
  const consoleCmd = vscode.commands.registerCommand('mmbasic.openConsole', openConsole);
  context.subscriptions.push(disposable, consoleCmd);
}

function deactivate() {}

module.exports = { activate, deactivate };
