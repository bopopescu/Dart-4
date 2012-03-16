package com.google.dart.tools.ui;

import org.eclipse.swt.graphics.Point;
import org.eclipse.swt.graphics.Rectangle;
import org.eclipse.swt.widgets.Display;
import org.eclipse.swt.widgets.Shell;
import org.eclipse.ui.IStartup;
import org.eclipse.ui.IViewPart;
import org.eclipse.ui.IViewReference;
import org.eclipse.ui.IWorkbenchPage;
import org.eclipse.ui.IWorkbenchWindow;
import org.eclipse.ui.PlatformUI;
import org.eclipse.ui.plugin.AbstractUIPlugin;

public class Activator extends AbstractUIPlugin implements IStartup {

  public Activator() {
  }

  @Override
  public void earlyStartup() {
    Display.getDefault().asyncExec(new Runnable() {
      @Override
      public void run() {
        configureEclipseWindowLocation();
        closeAllEditors();
        closeAllViews();
      }
    });
  }

  ////////////////////////////////////////////////////////////////////////////
  //
  // Workbench
  //
  ////////////////////////////////////////////////////////////////////////////

  /**
   * Ensures that Eclipse main window is in top-right corner of screen.
   */
  private void configureEclipseWindowLocation() {
    int x = 450;
    int y = 0;
    Shell shell = getActiveWorkbenchWindow().getShell();
    Point shellLocation = shell.getLocation();
    if (shellLocation.x != x || shellLocation.y != y) {
      Rectangle clientArea = Display.getDefault().getClientArea();
      shell.setBounds(x, y, clientArea.width - x, clientArea.height - 350);
      waitEventLoop(100, 10);
    }
  }

  private static IWorkbenchWindow getActiveWorkbenchWindow() {
    return PlatformUI.getWorkbench().getActiveWorkbenchWindow();
  }

  /**
   * Closes all {@link IViewPart}s.
   */
  public static void closeAllViews() {
    // run event loop to allow any async's be executed
    while (Display.getCurrent().readAndDispatch()) {
      // do nothing
    }
    // do close
    IWorkbenchPage activePage = getActiveWorkbenchWindow().getActivePage();
    IViewReference[] viewReferences = activePage.getViewReferences();
    if (viewReferences.length != 0) {
      for (IViewReference viewReference : viewReferences) {
        activePage.hideView(viewReference);
      }
      waitEventLoop(100);
    }
  }

  /**
   * Closes all editors.
   */
  public static void closeAllEditors() {
    // run event loop to allow any async's be executed
    while (Display.getCurrent().readAndDispatch()) {
      // do nothing
    }
    // close all editors
    {
      IWorkbenchPage activePage = getActiveWorkbenchWindow().getActivePage();
      activePage.closeAllEditors(false);
    }
  }

  ////////////////////////////////////////////////////////////////////////////
  //
  // UI
  //
  ////////////////////////////////////////////////////////////////////////////
  /**
   * Waits given number of milliseconds and runs events loop every 1 millisecond. At least one
   * events loop will be executed.
   */
  public static void waitEventLoop(int time) {
    waitEventLoop(time, 0);
  }

  /**
   * Waits given number of milliseconds and runs events loop every <code>sleepMillis</code>
   * milliseconds. At least one events loop will be executed.
   */
  public static void waitEventLoop(int time, long sleepMillis) {
    long start = System.currentTimeMillis();
    do {
      try {
        Thread.sleep(sleepMillis);
      } catch (Throwable e) {
      }
      while (Display.getCurrent().readAndDispatch()) {
        // do nothing
      }
    } while (System.currentTimeMillis() - start < time);
  }

}
