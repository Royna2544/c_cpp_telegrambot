package com.royna.tgbotclient

import android.content.Intent
import android.os.Build
import android.os.Bundle
import android.view.Menu
import android.view.MenuItem
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.drawerlayout.widget.DrawerLayout
import androidx.navigation.findNavController
import androidx.navigation.ui.AppBarConfiguration
import androidx.navigation.ui.navigateUp
import androidx.navigation.ui.setupActionBarWithNavController
import androidx.navigation.ui.setupWithNavController
import com.google.android.material.navigation.NavigationView
import com.google.android.material.snackbar.Snackbar
import com.royna.tgbotclient.databinding.ActivityMainBinding
import com.royna.tgbotclient.pm.IStoragePermission
import com.royna.tgbotclient.pm.StoragePermissionPreR
import com.royna.tgbotclient.pm.StoragePermissionR
import com.royna.tgbotclient.ui.settings.SettingsActivity

class MainActivity : AppCompatActivity() {
    private lateinit var appBarConfiguration: AppBarConfiguration
    private lateinit var binding: ActivityMainBinding

    private val storagePermission : IStoragePermission =
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R)
            StoragePermissionR() else StoragePermissionPreR()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        setSupportActionBar(binding.appBarMain.toolbar)
        storagePermission.init(this, object : IStoragePermission.IRequestResult {
            override fun onGranted() {
                Toast.makeText(this@MainActivity,
                    "Permission granted", Toast.LENGTH_SHORT).show()
            }

            override fun onDenied() {
                Toast.makeText(this@MainActivity,
                    "Permission denied", Toast.LENGTH_SHORT).show()
            }
        })

        if (storagePermission.isGranted(this))
            binding.appBarMain.fab.hide()

        binding.appBarMain.fab.setOnClickListener { view ->
            Snackbar.make(view, "I need storage permission", Snackbar.LENGTH_LONG)
                .setAction("Action", null)
                .setAnchorView(R.id.fab).show()
            storagePermission.request()
        }
        val drawerLayout: DrawerLayout = binding.drawerLayout
        val navView: NavigationView = binding.navView
        val navController = findNavController(R.id.nav_host_fragment_content_main)
        // Passing each menu ID as a set of Ids because each
        // menu should be considered as top level destinations.
        appBarConfiguration = AppBarConfiguration(
            setOf(
                R.id.nav_home, R.id.nav_send_msg, R.id.nav_uptime,
                R.id.nav_upload
            ), drawerLayout
        )
        setupActionBarWithNavController(navController, appBarConfiguration)
        navView.setupWithNavController(navController)
    }

    override fun onCreateOptionsMenu(menu: Menu): Boolean {
        // Inflate the menu; this adds items to the action bar if it is present.
        menuInflater.inflate(R.menu.main, menu)
        return true
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        when (item.itemId) {
            R.id.action_settings -> {
                startActivity(Intent(this, SettingsActivity::class.java))
            }
        }
        return super.onOptionsItemSelected(item)
    }

    override fun onSupportNavigateUp(): Boolean {
        val navController = findNavController(R.id.nav_host_fragment_content_main)
        return navController.navigateUp(appBarConfiguration) || super.onSupportNavigateUp()
    }
}