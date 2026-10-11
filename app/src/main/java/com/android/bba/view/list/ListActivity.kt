package com.android.bba.view.list

import android.app.Activity
import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.view.Menu
import android.view.MenuItem
import android.view.inputmethod.InputMethodManager
import androidx.activity.result.contract.ActivityResultContracts
import androidx.lifecycle.ViewModelProvider
import androidx.recyclerview.widget.LinearLayoutManager
import cbfg.rvadapter.RVAdapter
import com.ferfalk.simplesearchview.SimpleSearchView
import com.android.bba.R
import com.android.bba.bean.InstalledAppBean
import com.android.bba.databinding.ActivityListBinding
import com.android.bba.util.InjectionUtil
import com.android.bba.util.InstalledAppsPermission
import com.android.bba.util.inflate
import com.android.bba.util.toast
import com.android.bba.view.base.BaseActivity

class ListActivity : BaseActivity() {

    private val viewBinding: ActivityListBinding by inflate()

    private lateinit var mAdapter: RVAdapter<InstalledAppBean>

    private lateinit var viewModel: ListViewModel

    private var appList: List<InstalledAppBean> = ArrayList()

    private var pendingUserId = 0

    private val installedAppsPermissionLauncher =
            registerForActivityResult(ActivityResultContracts.RequestPermission()) { granted ->
                if (granted || !InstalledAppsPermission.needsRequest(this)) {
                    viewModel.getInstallAppList(pendingUserId)
                } else {
                    viewBinding.stateView.showEmpty()
                    toast(R.string.installed_apps_permission_denied)
                }
            }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(viewBinding.root)

        initToolbar(viewBinding.toolbarLayout.toolbar, R.string.installed_app, true)

        mAdapter =
                RVAdapter<InstalledAppBean>(this, ListAdapter())
                        .bind(viewBinding.recyclerView)
                        .setItemClickListener { _, item, _ -> finishWithResult(item.packageName) }

        viewBinding.recyclerView.layoutManager = LinearLayoutManager(this)

        initSearchView()
        initViewModel()
    }

    private fun initSearchView() {
        viewBinding.searchView.setOnQueryTextListener(
                object : SimpleSearchView.OnQueryTextListener {
                    override fun onQueryTextChange(newText: String): Boolean {
                        filterApp(newText)
                        return true
                    }

                    override fun onQueryTextCleared(): Boolean {
                        return true
                    }

                    override fun onQueryTextSubmit(query: String): Boolean {
                        return true
                    }
                }
        )
    }

    private fun initViewModel() {
        viewModel =
                ViewModelProvider(this, InjectionUtil.getListFactory())
                        .get(ListViewModel::class.java)
        pendingUserId = intent.getIntExtra("userID", 0)
        viewBinding.toolbarLayout.toolbar.setTitle(R.string.installed_app)
        if (InstalledAppsPermission.needsRequest(this)) {
            viewBinding.stateView.showLoading()
            installedAppsPermissionLauncher.launch(InstalledAppsPermission.PERMISSION)
        } else {
            viewModel.getInstallAppList(pendingUserId)
        }

        viewModel.loadingLiveData.observe(this) {
            if (it) {
                viewBinding.stateView.showLoading()
            } else {
                viewBinding.stateView.showContent()
            }
        }

        viewModel.appsLiveData.observe(this) {
            if (it != null) {
                this.appList = it
                viewBinding.searchView.setQuery("", false)
                filterApp("")
                if (it.isNotEmpty()) {
                    viewBinding.stateView.showContent()
                    viewModel.previewInstalledList()
                } else {
                    viewBinding.stateView.showEmpty()
                }
            }
        }
    }

    private fun filterApp(newText: String) {
        val newList =
                this.appList.filter {
                    it.name.contains(newText, true) or it.packageName.contains(newText, true)
                }
        mAdapter.setItems(newList)
    }

    private val openDocumentedResult =
            registerForActivityResult(ActivityResultContracts.GetContent()) {
                it?.run { finishWithResult(it.toString()) }
            }

    private fun finishWithResult(source: String) {
        intent.putExtra("source", source)
        setResult(Activity.RESULT_OK, intent)
        val imm: InputMethodManager = getSystemService(INPUT_METHOD_SERVICE) as InputMethodManager
        window.peekDecorView()?.run { imm.hideSoftInputFromWindow(windowToken, 0) }
        finish()
    }

    override fun onBackPressed() {
        if (viewBinding.searchView.isSearchOpen) {
            viewBinding.searchView.closeSearch()
        } else {
            super.onBackPressed()
        }
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        if (item.itemId == R.id.list_choose) {
            openDocumentedResult.launch("application/vnd.android.package-archive")
        }
        return true
    }

    override fun onCreateOptionsMenu(menu: Menu?): Boolean {
        menuInflater.inflate(R.menu.menu_list, menu)
        val item = menu!!.findItem(R.id.list_search)
        viewBinding.searchView.setMenuItem(item)

        return true
    }

    override fun onStop() {
        super.onStop()
        viewModel.loadingLiveData.postValue(true)
        viewModel.loadingLiveData.removeObservers(this)
        viewModel.appsLiveData.postValue(null)
        viewModel.appsLiveData.removeObservers(this)
    }

    companion object {
        fun start(context: Context) {
            val intent = Intent(context, ListActivity::class.java)
            context.startActivity(intent)
        }
    }
}
