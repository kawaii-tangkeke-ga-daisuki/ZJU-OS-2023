/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Header that contains the code (mostly headers) for which Rust bindings
 * will be automatically generated by `bindgen`.
 *
 * Sorted alphabetically.
 */

#include <kunit/test.h>
#include <linux/errname.h>
#include <linux/ethtool.h>
#include <linux/mdio.h>
#include <linux/phy.h>
#include <linux/slab.h>
#include <linux/refcount.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/workqueue.h>

/* `bindgen` gets confused at certain things. */
const size_t RUST_CONST_HELPER_ARCH_SLAB_MINALIGN = ARCH_SLAB_MINALIGN;
const gfp_t RUST_CONST_HELPER_GFP_KERNEL = GFP_KERNEL;
const gfp_t RUST_CONST_HELPER___GFP_ZERO = __GFP_ZERO;
