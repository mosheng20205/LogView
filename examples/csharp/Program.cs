using System;
using System.Drawing;
using System.Runtime.InteropServices;
using System.Text;
using System.Windows.Forms;

internal static class LogViewNative
{
    private const string DllName = "LogView_x64.dll";

    [DllImport(DllName, CallingConvention = CallingConvention.StdCall)]
    public static extern int LogView_Init();

    [DllImport(DllName, CallingConvention = CallingConvention.StdCall)]
    public static extern int LogView_Uninit();

    [DllImport(DllName, CallingConvention = CallingConvention.StdCall)]
    public static extern UIntPtr LogView_CreatePopupEx(int width, int height, int alpha);

    [DllImport(DllName, CallingConvention = CallingConvention.StdCall)]
    public static extern UIntPtr LogView_CreateChildEx(UIntPtr parentHwnd, int x, int y, int width, int height, int alpha);

    [DllImport(DllName, CallingConvention = CallingConvention.StdCall)]
    public static extern int LogView_Destroy(UIntPtr handle);

    [DllImport(DllName, CallingConvention = CallingConvention.StdCall)]
    public static extern int LogView_Show(UIntPtr handle);

    [DllImport(DllName, CallingConvention = CallingConvention.StdCall)]
    public static extern int LogView_SetTopMost(UIntPtr handle, int enable);

    [DllImport(DllName, CallingConvention = CallingConvention.StdCall)]
    public static extern int LogView_Clear(UIntPtr handle);

    [DllImport(DllName, CallingConvention = CallingConvention.StdCall)]
    public static extern int LogView_SetRect(UIntPtr handle, int x, int y, int width, int height);

    [DllImport(DllName, CallingConvention = CallingConvention.StdCall)]
    public static extern int LogView_SetTitle(UIntPtr handle, byte[] text);

    [DllImport(DllName, CallingConvention = CallingConvention.StdCall)]
    public static extern int LogView_AddLine(UIntPtr handle, byte[] text);

    [DllImport(DllName, CallingConvention = CallingConvention.StdCall)]
    public static extern int LogView_AddLineEx(UIntPtr handle, byte[] text, uint color, int level);

    [DllImport(DllName, CallingConvention = CallingConvention.StdCall)]
    public static extern IntPtr LogView_GetLastErrorText();
}

internal sealed class PatternPanel : Panel
{
    private const int WmPrintClient = 0x0318;

    public PatternPanel()
    {
        DoubleBuffered = true;
        ResizeRedraw = true;
    }

    protected override void OnPaint(PaintEventArgs e)
    {
        base.OnPaint(e);
        DrawPattern(e.Graphics);
    }

    protected override void WndProc(ref Message m)
    {
        if (m.Msg == WmPrintClient && m.WParam != IntPtr.Zero)
        {
            using Graphics graphics = Graphics.FromHdc(m.WParam);
            DrawPattern(graphics);
            return;
        }

        base.WndProc(ref m);
    }

    private void DrawPattern(Graphics graphics)
    {
        graphics.SetClip(ClientRectangle);
        graphics.Clear(Color.FromArgb(52, 62, 74));

        using var stripePen = new Pen(Color.FromArgb(120, 98, 178, 255), 2f);
        for (int x = -Height; x < Width + Height; x += 28)
        {
            graphics.DrawLine(stripePen, x, Height, x + Height, 0);
        }

        using var blockBrush = new SolidBrush(Color.FromArgb(90, 56, 189, 248));
        const int block = 52;
        for (int y = 18; y < Height; y += block * 2)
        {
            for (int x = 18; x < Width; x += block * 2)
            {
                graphics.FillRectangle(blockBrush, x, y, block, block);
            }
        }
    }
}

internal sealed class MainForm : Form
{
    private readonly Panel embeddedHost = new();
    private readonly PatternPanel transparentEmbeddedHost = new();
    private readonly Button clearEmbeddedButton = new();
    private readonly Button clearTransparentButton = new();
    private readonly Button clearPopupButton = new();
    private readonly Button showPopupButton = new();
    private readonly System.Windows.Forms.Timer timer = new();
    private UIntPtr embeddedLog;
    private UIntPtr transparentEmbeddedLog;
    private UIntPtr popupLog;
    private int tick;

    public MainForm()
    {
        AutoScaleMode = AutoScaleMode.Dpi;
        Text = "LogView C# 内嵌窗口 + 透明内嵌窗口 + 独立窗口示例";
        StartPosition = FormStartPosition.CenterScreen;
        Size = new Size(1040, 640);
        MinimumSize = new Size(820, 500);

        var toolbar = new FlowLayoutPanel
        {
            Dock = DockStyle.Top,
            AutoSize = true,
            AutoSizeMode = AutoSizeMode.GrowAndShrink,
            Padding = new Padding(12, 8, 12, 8),
            FlowDirection = FlowDirection.LeftToRight,
            WrapContents = false
        };

        ConfigureToolbarButton(clearEmbeddedButton, "清空内嵌日志");
        clearEmbeddedButton.Click += (_, _) =>
        {
            if (embeddedLog != UIntPtr.Zero)
            {
                Check(LogViewNative.LogView_Clear(embeddedLog));
            }
        };

        ConfigureToolbarButton(clearTransparentButton, "清空透明日志");
        clearTransparentButton.Click += (_, _) =>
        {
            if (transparentEmbeddedLog != UIntPtr.Zero)
            {
                Check(LogViewNative.LogView_Clear(transparentEmbeddedLog));
            }
        };

        ConfigureToolbarButton(clearPopupButton, "清空独立日志");
        clearPopupButton.Click += (_, _) =>
        {
            if (popupLog != UIntPtr.Zero)
            {
                Check(LogViewNative.LogView_Clear(popupLog));
            }
        };

        ConfigureToolbarButton(showPopupButton, "显示独立窗口");
        showPopupButton.Click += (_, _) => ShowPopupLog();

        toolbar.Controls.Add(clearEmbeddedButton);
        toolbar.Controls.Add(clearTransparentButton);
        toolbar.Controls.Add(clearPopupButton);
        toolbar.Controls.Add(showPopupButton);

        var content = new TableLayoutPanel
        {
            Dock = DockStyle.Fill,
            ColumnCount = 2,
            RowCount = 1,
            BackColor = Color.FromArgb(24, 27, 31),
            Padding = new Padding(8)
        };
        content.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 50f));
        content.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 50f));
        content.RowStyles.Add(new RowStyle(SizeType.Percent, 100f));

        embeddedHost.Dock = DockStyle.Fill;
        embeddedHost.BackColor = Color.FromArgb(32, 36, 40);
        embeddedHost.Margin = new Padding(0, 0, 4, 0);
        embeddedHost.Padding = new Padding(0);

        transparentEmbeddedHost.Dock = DockStyle.Fill;
        transparentEmbeddedHost.Margin = new Padding(4, 0, 0, 0);
        transparentEmbeddedHost.Padding = new Padding(0);

        content.Controls.Add(embeddedHost, 0, 0);
        content.Controls.Add(transparentEmbeddedHost, 1, 0);

        Controls.Add(content);
        Controls.Add(toolbar);

        Shown += OnShown;
        FormClosing += OnClosing;
        embeddedHost.Resize += (_, _) => ResizeEmbeddedLogs();
        transparentEmbeddedHost.Resize += (_, _) => ResizeEmbeddedLogs();
        DpiChanged += (_, _) => BeginInvoke(ResizeEmbeddedLogs);

        timer.Interval = 120;
        timer.Tick += (_, _) => AppendLogs();
    }

    private static void ConfigureToolbarButton(Button button, string text)
    {
        button.Text = text;
        button.AutoSize = true;
        button.AutoSizeMode = AutoSizeMode.GrowAndShrink;
        button.MinimumSize = new Size(0, 30);
        button.Padding = new Padding(10, 3, 10, 3);
        button.Margin = new Padding(0, 0, 8, 0);
        button.UseVisualStyleBackColor = true;
    }

    private void OnShown(object? sender, EventArgs e)
    {
        BeginInvoke(CreateLogWindows);
    }

    private void CreateLogWindows()
    {
        Check(LogViewNative.LogView_Init());
        embeddedHost.CreateControl();
        transparentEmbeddedHost.CreateControl();

        embeddedLog = LogViewNative.LogView_CreateChildEx(
            (UIntPtr)embeddedHost.Handle,
            0,
            0,
            Math.Max(1, embeddedHost.ClientSize.Width),
            Math.Max(1, embeddedHost.ClientSize.Height),
            255);
        if (embeddedLog == UIntPtr.Zero)
        {
            Check(0);
        }

        transparentEmbeddedLog = LogViewNative.LogView_CreateChildEx(
            (UIntPtr)transparentEmbeddedHost.Handle,
            0,
            0,
            Math.Max(1, transparentEmbeddedHost.ClientSize.Width),
            Math.Max(1, transparentEmbeddedHost.ClientSize.Height),
            128);
        if (transparentEmbeddedLog == UIntPtr.Zero)
        {
            Check(0);
        }

        popupLog = LogViewNative.LogView_CreatePopupEx(520, 420, 190);
        if (popupLog == UIntPtr.Zero)
        {
            Check(0);
        }

        Check(LogViewNative.LogView_SetTitle(embeddedLog, Utf8("C# 内嵌日志窗口")));
        Check(LogViewNative.LogView_SetTitle(transparentEmbeddedLog, Utf8("C# 50% 透明内嵌日志窗口")));
        Check(LogViewNative.LogView_SetTitle(popupLog, Utf8("C# 独立悬浮日志窗口")));
        Check(LogViewNative.LogView_Show(embeddedLog));
        Check(LogViewNative.LogView_Show(transparentEmbeddedLog));
        ShowPopupLog();

        ResizeEmbeddedLogs();
        timer.Start();
    }

    private void ShowPopupLog()
    {
        if (popupLog == UIntPtr.Zero)
        {
            return;
        }

        Check(LogViewNative.LogView_Show(popupLog));
        Check(LogViewNative.LogView_SetTopMost(popupLog, 1));
    }

    private void AppendLogs()
    {
        tick++;
        Check(LogViewNative.LogView_AddLine(embeddedLog, Utf8($"内嵌日志 {tick:000}")));
        Check(LogViewNative.LogView_AddLine(transparentEmbeddedLog, Utf8($"50% 透明内嵌日志 {tick:000}")));

        uint color;
        int level;
        string text;
        if (tick % 9 == 0)
        {
            color = 0xFFEF4444;
            level = 3;
            text = $"独立窗口错误日志 {tick:000}";
        }
        else if (tick % 5 == 0)
        {
            color = 0xFFF59E0B;
            level = 2;
            text = $"独立窗口警告日志 {tick:000}";
        }
        else
        {
            color = 0xFF38D27A;
            level = 1;
            text = $"独立窗口成功日志 {tick:000}";
        }
        Check(LogViewNative.LogView_AddLineEx(popupLog, Utf8(text), color, level));
    }

    private void ResizeEmbeddedLogs()
    {
        if (embeddedLog != UIntPtr.Zero && embeddedHost.IsHandleCreated)
        {
            Check(LogViewNative.LogView_SetRect(
                embeddedLog,
                0,
                0,
                Math.Max(1, embeddedHost.ClientSize.Width),
                Math.Max(1, embeddedHost.ClientSize.Height)));
        }

        if (transparentEmbeddedLog != UIntPtr.Zero && transparentEmbeddedHost.IsHandleCreated)
        {
            Check(LogViewNative.LogView_SetRect(
                transparentEmbeddedLog,
                0,
                0,
                Math.Max(1, transparentEmbeddedHost.ClientSize.Width),
                Math.Max(1, transparentEmbeddedHost.ClientSize.Height)));
        }
    }

    private void OnClosing(object? sender, FormClosingEventArgs e)
    {
        timer.Stop();
        if (embeddedLog != UIntPtr.Zero)
        {
            LogViewNative.LogView_Destroy(embeddedLog);
            embeddedLog = UIntPtr.Zero;
        }
        if (transparentEmbeddedLog != UIntPtr.Zero)
        {
            LogViewNative.LogView_Destroy(transparentEmbeddedLog);
            transparentEmbeddedLog = UIntPtr.Zero;
        }
        if (popupLog != UIntPtr.Zero)
        {
            LogViewNative.LogView_Destroy(popupLog);
            popupLog = UIntPtr.Zero;
        }
        LogViewNative.LogView_Uninit();
    }

    private static byte[] Utf8(string text) => Encoding.UTF8.GetBytes(text + "\0");

    private static void Check(int ok)
    {
        if (ok == 0)
        {
            string message = Marshal.PtrToStringUTF8(LogViewNative.LogView_GetLastErrorText()) ?? "LogView call failed.";
            throw new InvalidOperationException(message);
        }
    }
}

internal static class Program
{
    [STAThread]
    private static void Main()
    {
        ApplicationConfiguration.Initialize();
        Application.Run(new MainForm());
    }
}
