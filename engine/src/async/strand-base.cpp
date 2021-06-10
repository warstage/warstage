// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include "./strand-base.h"
#include "./strand-manual.h"


std::mutex Strand_base::mutex__;
std::shared_ptr<Strand_Manual> Strand_base::render__;
thread_local std::shared_ptr<Strand_base> Strand_base::current__;


std::shared_ptr<Strand_Manual> Strand_base::getRender() {
    std::lock_guard lock{mutex__};
    if (!render__) {
        render__ = std::make_shared<Strand_Manual>();
    }
    return render__;
}
