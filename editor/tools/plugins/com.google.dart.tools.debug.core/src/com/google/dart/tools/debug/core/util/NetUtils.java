/*
 * Copyright (c) 2012, the Dart project authors.
 * 
 * Licensed under the Eclipse Public License v1.0 (the "License"); you may not use this file except
 * in compliance with the License. You may obtain a copy of the License at
 * 
 * http://www.eclipse.org/legal/epl-v10.html
 * 
 * Unless required by applicable law or agreed to in writing, software distributed under the License
 * is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express
 * or implied. See the License for the specific language governing permissions and limitations under
 * the License.
 */
package com.google.dart.tools.debug.core.util;

import java.io.IOException;
import java.net.ServerSocket;
import java.net.URI;

/**
 * A collection of static networking utilities.
 */
public class NetUtils {

  /**
   * Compares whether two uris are equal. This handles the case where file urls are specified
   * slightly differently (file:/ vs file:///).
   * 
   * @param url1
   * @param url2
   * @return
   */
  public static boolean compareUrls(String url1, String url2) {
    if (url1.equals(url2)) {
      return true;
    }

    URI u1 = URI.create(url1);
    URI u2 = URI.create(url2);

    return u1.equals(u2);
  }

  /**
   * Find and return an unused server socket port. Attempt to use preferredPort; if that is not
   * available then we return any unused port.
   * 
   * @param preferredPort
   * @return
   */
  public static int findUnusedPort(int preferredPort) {
    try {
      // Bind to any free port.
      ServerSocket ss = new ServerSocket(0);

      int port = ss.getLocalPort();

      ss.close();

      return port;
    } catch (IOException ioe) {

    }

    return -1;
  }

  private NetUtils() {

  }

}
