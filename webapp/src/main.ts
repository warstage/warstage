// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

import { platformBrowserDynamic } from '@angular/platform-browser-dynamic';

import { AppModule } from './app/app.module';

platformBrowserDynamic().bootstrapModule(AppModule)
  .catch(err => console.error(err));
