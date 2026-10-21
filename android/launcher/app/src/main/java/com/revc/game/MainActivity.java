package com.revc.game;

import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.provider.DocumentsContract;
import android.widget.Button;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;

import org.libsdl.app.SDLActivity;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/** The launcher installs reVC's own data and imports the player's game files. */
public class MainActivity extends AppCompatActivity {
    private static final int CHOOSE_GAME_FOLDER = 1;
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
    private int importedFiles;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        status = findViewById(R.id.game_status);
        importButton = findViewById(R.id.import_game);
        playButton = findViewById(R.id.play_game);
        gameRoot = getExternalFilesDir(null);
        if (gameRoot == null) gameRoot = getFilesDir();

        importButton.setOnClickListener(view -> {
            Intent choose = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
            choose.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
            startActivityForResult(choose, CHOOSE_GAME_FOLDER);
        });
        playButton.setOnClickListener(view -> {
            if (!hasGameFiles()) {
                status.setText(R.string.game_files_missing);
                return;
            }
            Intent game = new Intent(this, SDLActivity.class);
            game.putExtra("revc.gameDir", gameRoot.getAbsolutePath());
            startActivity(game);
        });
        runInBackground(() -> installSupportFiles());
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
            installSupportFiles();
        });
    }

    private interface BackgroundJob { void run() throws IOException; }

    private void runInBackground(BackgroundJob job) {
        importButton.setEnabled(false);
        playButton.setEnabled(false);
        status.setText(R.string.preparing_game);
        io.execute(() -> {
            String error = null;
            try {
                if (!gameRoot.isDirectory() && !gameRoot.mkdirs())
                    throw new IOException("Cannot create " + gameRoot);
                job.run();
            } catch (IOException | SecurityException e) {
                error = e.getMessage() == null ? e.toString() : e.getMessage();
            }
            boolean ready = hasGameFiles();
            String failure = error;
            runOnUiThread(() -> {
                importButton.setEnabled(true);
                playButton.setEnabled(ready);
                if (failure != null)
                    status.setText(getString(R.string.import_failed, failure));
                else if (ready)
                    status.setText(getString(R.string.game_ready, gameRoot.getAbsolutePath()));
                else
                    status.setText(R.string.game_files_missing);
            });
        });
    }

    // Android's document picker grants content:// access, not a filesystem
    // path. Copy into our app-specific storage, which the native engine can
    // actually open via POSIX file APIs (no all-files-storage permission).
    private void importFolder(Uri tree, String folderId, File target, int depth) throws IOException {
        if (depth > 32) throw new IOException("Game folder is too deeply nested");
        Uri children = DocumentsContract.buildChildDocumentsUriUsingTree(tree, folderId);
        try (Cursor cursor = getContentResolver().query(children, DOCUMENT_COLUMNS, null, null, null)) {
            if (cursor == null) throw new IOException("Cannot list the selected folder");
            while (cursor.moveToNext()) {
                String id = cursor.getString(0);
                String name = cursor.getString(1);
                String mime = cursor.getString(2);
                checkName(name);
                File destination = new File(target, name);
                if (DocumentsContract.Document.MIME_TYPE_DIR.equals(mime)) {
                    if (!destination.isDirectory() && !destination.mkdirs())
                        throw new IOException("Cannot create " + destination);
                    importFolder(tree, id, destination, depth + 1);
                } else {
                    Uri document = DocumentsContract.buildDocumentUriUsingTree(tree, id);
                    try (InputStream input = getContentResolver().openInputStream(document)) {
                        if (input == null) throw new IOException("Cannot read " + name);
                        copyAtomically(input, destination);
                    }
                    ++importedFiles;
                    if (importedFiles % 32 == 0) {
                        int count = importedFiles;
                        runOnUiThread(() -> status.setText(getString(R.string.import_progress, count)));
                    }
                }
            }
        }
    }

    private void installSupportFiles() throws IOException {
        installAssetDirectory("", gameRoot);
    }

    private void installAssetDirectory(String assetPath, File target) throws IOException {
        String[] entries = getAssets().list(assetPath);
        if (entries == null) throw new IOException("Cannot list reVC support files");
        if (entries.length == 0) {
            try (InputStream input = getAssets().open(assetPath)) {
                copyAtomically(input, target);
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
            if (destination.exists() && !destination.delete())
                throw new IOException("Cannot replace " + destination);
            if (!temporary.renameTo(destination))
                throw new IOException("Cannot install " + destination);
        } finally {
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
        io.shutdown();
        super.onDestroy();
    }
}
