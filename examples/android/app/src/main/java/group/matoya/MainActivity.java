package group.matoya;

import android.app.Activity;
import android.view.View;
import android.os.Bundle;
import android.util.Log;

import group.matoya.lib.Matoya;

public class MainActivity extends Activity {
	Matoya mty;

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);

		this.mty = new Matoya(this);
	}

	@Override
	protected void onDestroy() {
		super.onDestroy();

		this.mty.destroy();
		this.mty = null;
	}
}
