package com.revc.game;

import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.database.Cursor;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.provider.DocumentsContract;
import android.widget.Button;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;

import org.libsdl.app.SDLActivity;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.RejectedExecutionException;

/** The launcher installs reVC's own data and imports the player's game files. */
public class MainActivity extends AppCompatActivity {
    private static final int CHOOSE_GAME_FOLDER = 1;
    private static final String STAMP_NAME = ".revc-support-version";
    private static final int MAX_DEPTH = 32;
    private static final String[] DOCUMENT_COLUMNS = {
            DocumentsContract.Document.COLUMN_DOCUMENT_ID,
            DocumentsContract.Document.COLUMN_DISPLAY_NAME,
            DocumentsContract.Document.COLUMN_MIME_TYPE
    };

    private final ExecutorService io = Executors.newSingleThreadExecutor();
    private File gameRoot;
    private TextView status;
    private Button importButton;
    private Button playButton;
    private volatile int importedFiles;
    private volatile boolean busy;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE);
        setContentView(R.layout.activity_main);
        status = findViewById(R.id.game_status);
        importButton = findViewById(R.id.import_game);
        playButton = findViewById(R.id.play_game);
        gameRoot = getExternalFilesDir(null);
        if (gameRoot == null) gameRoot = getFilesDir();

        importButton.setOnClickListener(view -> {
            if (busy) return;
            Intent choose = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
            choose.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
            choose.addFlags(Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
            try {
                startActivityForResult(choose, CHOOSE_GAME_FOLDER);
            } catch (Exception e) {
                status.setText(getString(R.string.import_failed,
                        e.getMessage() == null ? e.toString() : e.getMessage()));
            }
        });
        playButton.setOnClickListener(view -> {
            if (busy) return;
            if (!hasGameFiles()) {
                status.setText(R.string.game_files_missing);
                playButton.setEnabled(false);
                return;
            }
            Intent game = new Intent(this, SDLActivity.class);
            game.putExtra(SDLActivity.EXTRA_GAME_DIR, gameRoot.getAbsolutePath());
            startActivity(game);
        });

        // The support files only have to be unpacked once per app version; the
        // previous implementation recopied the whole asset tree on every start.
        runInBackground(() -> installSupportFiles(false), true);
    }

    @Override
    protected void onResume() {
        super.onResume();
        // Returning from the game (or from the picker) must leave the buttons
        // in a consistent state even when a job already finished.
        if (!busy) {
            boolean ready = hasGameFiles();
            playButton.setEnabled(ready);
            if (ready && status != null && status.getText().length() == 0)
                status.setText(getString(R.string.game_ready, gameRoot.getAbsolutePath()));
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, @Nullable Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode != CHOOSE_GAME_FOLDER || resultCode != RESULT_OK ||
                data == null || data.getData() == null) return;

        Uri tree = data.getData();
        runInBackground(() -> {
            importedFiles = 0;
            importFolder(tree, DocumentsContract.getTreeDocumentId(tree), gameRoot, 0);
            // The game's own data is copyrighted and not included in the APK;
            // the small files in gamefiles/ are reVC replacements/extra files.
            // They always win, so refresh them after a fresh import.
            installSupportFiles(true);
        }, false);
    }

    private interface BackgroundJob { void run() throws IOException; }

    private void runInBackground(BackgroundJob job, boolean quiet) {
        if (io.isShutdown()) return;
        busy = true;
        importButton.setEnabled(false);
        playButton.setEnabled(false);
        if (!quiet) status.setText(R.string.preparing_game);
        final boolean initial = quiet;
        try {
            io.execute(() -> {
                String error = null;
                try {
                    if (!gameRoot.isDirectory() && !gameRoot.mkdirs())
                        throw new IOException("Cannot create " + gameRoot);
                    job.run();
                } catch (IOException | SecurityException e) {
                    error = e.getMessage() == null ? e.toString() : e.getMessage();
                }
                final String failure = error;
                final boolean ready = error == null && hasGameFiles();
                busy = false;
                runOnUiThread(() -> {
                    if (isDestroyed() || isFinishing()) return;
                    importButton.setEnabled(true);
                    playButton.setEnabled(ready);
                    if (failure != null)
                        status.setText(getString(R.string.import_failed, failure));
                    else if (ready)
                        status.setText(getString(R.string.game_ready, gameRoot.getAbsolutePath()));
                    else if (!initial)
                        status.setText(R.string.game_files_missing);
                    else
                        status.setText(R.string.game_files_missing);
                });
            });
        } catch (RejectedExecutionException e) {
            busy = false;
        }
    }

    // Android's document picker grants content:// access, not a filesystem
    // path. Copy into our app-specific storage, which the native engine can
    // actually open via POSIX file APIs (no all-files-storage permission).
    private void importFolder(Uri tree, String folderId, File target, int depth) throws IOException {
        if (depth > MAX_DEPTH) throw new IOException("Game folder is too deeply nested");
        if (!target.isDirectory() && !target.mkdirs())
            throw new IOException("Cannot create " + target);
        Uri children = DocumentsContract.buildChildDocumentsUriUsingTree(tree, folderId);
        try (Cursor cursor = getContentResolver().query(children, DOCUMENT_COLUMNS, null, null, null)) {
            if (cursor == null) throw new IOException("Cannot list the selected folder");
            while (cursor.moveToNext()) {
                String id = cursor.getString(0);
                String name = cursor.getString(1);
                String mime = cursor.getString(2);
                checkName(name);
                File destination = new File(target, name);
                // Empty folders are still folders: trust the provider's MIME
                // type instead of guessing from the child count.
                if (DocumentsContract.Document.MIME_TYPE_DIR.equals(mime)) {
                    importFolder(tree, id, destination, depth + 1);
                } else {
                    Uri document = DocumentsContract.buildDocumentUriUsingTree(tree, id);
                    try (InputStream input = getContentResolver().openInputStream(document)) {
                        if (input == null) throw new IOException("Cannot read " + name);
                        copyAtomically(input, destination);
                    }
                    ++importedFiles;
                    if (importedFiles % 64 == 0) {
                        final int count = importedFiles;
                        runOnUiThread(() -> {
                            if (isDestroyed() || isFinishing()) return;
                            status.setText(getString(R.string.import_progress, count));
                        });
                    }
                }
            }
        }
    }

    /**
     * Unpacks the APK's reVC support files into the game directory.
     *
     * @param force reinstall even when the stamp matches (after a fresh import)
     */
    private void installSupportFiles(boolean force) throws IOException {
        File stamp = new File(gameRoot, STAMP_NAME);
        String version = appVersion();
        if (!force && stamp.isFile() && version.equals(readStamp(stamp))) return;
        installAssetDirectory("", gameRoot);
        writeStamp(stamp, version);
    }

    private String appVersion() {
        try {
            return getPackageManager().getPackageInfo(getPackageName(), 0).versionName
                    + ":" + Build.VERSION.SDK_INT;
        } catch (Exception e) {
            return "unknown";
        }
    }

    private static String readStamp(File stamp) {
        try (InputStream input = new java.io.FileInputStream(stamp)) {
            byte[] buffer = new byte[128];
            int read = input.read(buffer);
            return read <= 0 ? "" : new String(buffer, 0, read, "UTF-8").trim();
        } catch (IOException e) {
            return "";
        }
    }

    private static void writeStamp(File stamp, String value) {
        try (OutputStream output = new FileOutputStream(stamp)) {
            output.write(value.getBytes("UTF-8"));
        } catch (IOException ignored) {
            // A missing stamp only costs one extra unpack next launch.
        }
    }

    private void installAssetDirectory(String assetPath, File target) throws IOException {
        String[] entries = getAssets().list(assetPath);
        if (entries == null) throw new IOException("Cannot list reVC support files");
        if (entries.length == 0) {
            // AssetManager cannot tell an empty folder from a file, so try to
            // open it: only real files can be opened.
            try (InputStream probe = getAssets().open(assetPath)) {
                copyAtomically(probe, target);
            } catch (FileNotFoundException emptyFolder) {
                if (!target.isDirectory() && !target.mkdirs())
                    throw new IOException("Cannot create " + target);
            }
            return;
        }
        if (!target.isDirectory() && !target.mkdirs())
            throw new IOException("Cannot create " + target);
        for (String name : entries) {
            checkName(name);
            String child = assetPath.isEmpty() ? name : assetPath + "/" + name;
            installAssetDirectory(child, new File(target, name));
        }
    }

    private static void checkName(String name) throws IOException {
        if (name == null || name.isEmpty() || name.equals(".") || name.equals("..") ||
                name.indexOf('/') >= 0 || name.indexOf('\\') >= 0)
            throw new IOException("Invalid game file name: " + name);
    }

    /**
     * Copies into a sibling temporary file and only then replaces the target,
     * so an interrupted import can never leave a truncated game file behind.
     */
    private static void copyAtomically(InputStream source, File destination) throws IOException {
        File parent = destination.getParentFile();
        if (parent == null || (!parent.isDirectory() && !parent.mkdirs()))
            throw new IOException("Cannot create " + parent);
        File temporary = File.createTempFile(".revc-", ".tmp", parent);
        try {
            try (OutputStream output = new FileOutputStream(temporary)) {
                byte[] buffer = new byte[64 * 1024];
                int read;
                while ((read = source.read(buffer)) != -1)
                    output.write(buffer, 0, read);
            }
            if (temporary.renameTo(destination)) return;
            // renameTo does not replace an existing file on every filesystem.
            if (destination.exists() && !destination.delete())
                throw new IOException("Cannot replace " + destination);
            if (!temporary.renameTo(destination))
                throw new IOException("Cannot install " + destination);
        } finally {
            //noinspection ResultOfMethodCallIgnored
            temporary.delete();
        }
    }

    private boolean hasGameFiles() {
        File data = childIgnoringCase(gameRoot, "data");
        File models = childIgnoringCase(gameRoot, "models");
        File dat = childIgnoringCase(data, "gta_vc.dat");
        File img = childIgnoringCase(models, "gta3.img");
        return dat != null && dat.isFile() && dat.length() > 0 &&
                img != null && img.isFile() && img.length() > 0;
    }

    @Nullable
    private static File childIgnoringCase(@Nullable File parent, String name) {
        if (parent == null) return null;
        File[] files = parent.listFiles();
        if (files == null) return null;
        for (File file : files)
            if (file.getName().equalsIgnoreCase(name)) return file;
        return null;
    }

    @Override
    protected void onDestroy() {
        // Cancel a running import instead of letting it write into a directory
        // of a destroyed activity (and instead of leaking the worker thread).
        io.shutdownNow();
        super.onDestroy();
    }
}
