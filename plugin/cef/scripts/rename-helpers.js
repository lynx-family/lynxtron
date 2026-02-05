#!/usr/bin/env node

import fs from 'fs/promises';
import path from 'path';
import { fileURLToPath } from 'url';
import plist from 'plist';
import yaml from 'js-yaml';

/**
 * Helper application renaming script
 * Reads build.productName from the project package.json and renames the cef-webview Helper files
 */

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

async function main() {
  try {
    // 1. Get project root directory
    const projectRoot = path.resolve(__dirname, '../../../..'); // The package.json is in the same directory as the scripts folder
    
    // 2. Try to find electron-builder config files first
    const electronBuilderConfigs = [
      path.join(projectRoot, 'electron-builder.yml'),
      path.join(projectRoot, 'electron-builder.yaml'),
      path.join(projectRoot, 'electron-builder.json')
    ];
    
    let productName = null;
    let appId = null;
    
    // Check each electron-builder config file
    for (const configPath of electronBuilderConfigs) {
      try {
        await fs.access(configPath);
        console.log(`Found electron-builder config: ${configPath}`);
        
        // Read and parse the config file
        const configContent = await fs.readFile(configPath, 'utf8');
        let config;
        
        if (configPath.endsWith('.json')) {
          config = JSON.parse(configContent);
        } else {
          config = yaml.load(configContent);
        }
        
        // Extract productName and appId from config
        if (config.productName) {
          productName = config.productName;
          console.log(`Obtained productName from electron-builder config: ${productName}`);
        }
        
        if (config.appId) {
          appId = config.appId;
          console.log(`Obtained appId from electron-builder config: ${appId}`);
        }
        
        // If both productName and appId are found, we can break early
        if (productName && appId) {
          break;
        }
      } catch (error) {
        // Config file doesn't exist or can't be parsed, continue to next one
        continue;
      }
    }
    
    // 3. If productName or appId not found in electron-builder config, try package.json
    if (!productName || !appId) {
      const packageJsonPath = path.join(projectRoot, 'package.json');
      console.log(`Checking package.json: ${packageJsonPath}`);
      
      try {
        await fs.access(packageJsonPath);
      } catch {
        console.log(`Could not access package.json: ${packageJsonPath}`);
        return;
      }
      
      const packageContent = await fs.readFile(packageJsonPath, 'utf8');
      const packageJson = JSON.parse(packageContent);
      
      // Get build configuration
      const buildConfig = packageJson.build || {};
      
      // Get productName from package.json if not found earlier
      if (!productName) {
        productName = buildConfig.productName;
        
        if (!productName) {
          console.log('productName is not configured in electron-builder config or package.json');
          return;
        }
        
        console.log(`Obtained productName from package.json: ${productName}`);
      }
      
      // Get appId from package.json if not found earlier
      if (!appId) {
        appId = buildConfig.appId;
        
        if (appId) {
          console.log(`Obtained appId from package.json: ${appId}`);
        } else {
          // Use productName as fallback for appId if not configured
          appId = productName.toLowerCase();
          console.log(`No appId configured, using productName as fallback: ${appId}`);
        }
      }
    }
    
    // 4. Get cef-webview frameworks directory
    const currentPlatform = process.platform; // e.g., 'darwin', 'win32', 'linux'
    const currentArch = process.arch; // e.g., 'arm64', 'x64', 'ia32'
    const frameworksPath = path.join(__dirname, 
      `../dist/${currentPlatform}/${currentArch}/frameworks`);
    
    try {
      await fs.access(frameworksPath);
    } catch {
      throw new Error(`Could not find frameworks directory: ${frameworksPath}`);
    }
    
    // 5. Get all Helper files
    const files = await fs.readdir(frameworksPath);
    const helperApps = files.filter(file => file.startsWith('lynxtron Helper'));
    
    if (helperApps.length === 0) {
      console.log('No Helper files to rename found');
      return;
    }
    
    // 6. Rename each Helper file
    for (const app of helperApps) {
      const oldPath = path.join(frameworksPath, app);
      // Replace "lynxtron Helper" with "{productName} Helper"
      const newName = app.replace('lynxtron Helper', `${productName} Helper`);
      const newPath = path.join(frameworksPath, newName);
      
      console.log(`Renaming: ${app} -> ${newName}`);
      
      // Rename directory
      await fs.rename(oldPath, newPath);

      // Rename executable file
      const execOldName = app.replace('.app', '');
      const execOldPath = path.join(newPath, 'Contents/MacOS', execOldName);
      const execNewName = newName.replace('.app', '');
      const execNewPath = path.join(newPath, 'Contents/MacOS', execNewName);
      await fs.rename(execOldPath, execNewPath);

      // 7. Update the Info.plist file in each Helper app
      await updateHelperPlist(newPath, productName, appId);
    }
    
    console.log('\n✅ All Helper files renamed successfully!');
    
  } catch (error) {
    console.error('❌ Error occurred during renaming:', error.message);
    console.error(error.stack);
    process.exit(1);
  }
}

/**
 * Update the Info.plist file in Helper apps
 * @param {string} helperAppPath - Path to the Helper app
 * @param {string} productName - Product name
 * @param {string} appId - Application ID (bundle identifier)
 */
async function updateHelperPlist(helperAppPath, productName, appId) {
  const plistPath = path.join(helperAppPath, 'Contents/Info.plist');
  try {
    await fs.access(plistPath);
  } catch {
    console.warn(`⚠️  Could not find Info.plist: ${plistPath}`);
    return;
  }
  
  try {
    // Update Info.plist using universal Node.js approach
    const plistContent = await fs.readFile(plistPath, 'utf8');
    const plistData = plist.parse(plistContent);
    
    // Update CFBundleName
    plistData.CFBundleName = `${productName} Helper`;
    
    // Update CFBundleExecutable
    const executableName = path.basename(helperAppPath, '.app');
    plistData.CFBundleExecutable = executableName;
    
    // Update CFBundleIdentifier (keep bundle ID structure, only replace prefix)
    if (plistData.CFBundleIdentifier && plistData.CFBundleIdentifier.includes('org.cef.cefwebview.helper')) {
      // Replace the org.cef.cefwebview.helper prefix with appId
      plistData.CFBundleIdentifier = plistData.CFBundleIdentifier.replace('org.cef.cefwebview.helper', `${appId}.helper`);
    }
    
    // Write the updated plist back to file
    const updatedPlistContent = plist.build(plistData);
    await fs.writeFile(plistPath, updatedPlistContent, 'utf8');
    
  } catch (error) {
    console.warn(`⚠️  Failed to update Info.plist: ${plistPath}`);
    console.warn('Error message:', error.message);
  }
}

// Execute the script
main();
