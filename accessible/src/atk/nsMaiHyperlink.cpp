/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Sun Microsystems, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bolian Yin (bolian.yin@sun.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsIURI.h"
#include "nsMaiHyperlink.h"

/* MaiAtkHyperlink */

#define MAI_TYPE_ATK_HYPERLINK      (mai_atk_hyperlink_get_type ())
#define MAI_ATK_HYPERLINK(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj),\
                                     MAI_TYPE_ATK_HYPERLINK, MaiAtkHyperlink))
#define MAI_ATK_HYPERLINK_CLASS(klass)  (G_TYPE_CHECK_CLASS_CAST ((klass),\
                                 MAI_TYPE_ATK_HYPERLINK, MaiAtkHyperlinkClass))
#define MAI_IS_ATK_HYPERLINK(obj)      (G_TYPE_CHECK_INSTANCE_TYPE ((obj),\
                                        MAI_TYPE_ATK_HYPERLINK))
#define MAI_IS_ATK_HYPERLINK_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),\
                                        MAI_TYPE_ATK_HYPERLINK))
#define MAI_ATK_HYPERLINK_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj),\
                                 MAI_TYPE_ATK_HYPERLINK, MaiAtkHyperlinkClass))

/**
 * This MaiAtkHyperlink is a thin wrapper, in the MAI namespace,
 * for AtkHyperlink
 */

struct MaiAtkHyperlink
{
    AtkHyperlink parent;

    /*
     * The MaiHyperlink whose properties and features are exported via this
     * hyperlink instance.
     */
    MaiHyperlink *maiHyperlink;
};

struct MaiAtkHyperlinkClass
{
    AtkHyperlinkClass parent_class;
};

GType mai_atk_hyperlink_get_type(void);

G_BEGIN_DECLS
/* callbacks for AtkHyperlink */
static void classInitCB(AtkHyperlinkClass *aClass);
static void finalizeCB(GObject *aObj);

/* callbacks for AtkHyperlink virtual functions */
static gchar *getUriCB(AtkHyperlink *aLink, gint aLinkIndex);
static AtkObject *getObjectCB(AtkHyperlink *aLink, gint aLinkIndex);
static gint getEndIndexCB(AtkHyperlink *aLink);
static gint getStartIndexCB(AtkHyperlink *aLink);
static gboolean isValidCB(AtkHyperlink *aLink);
static gint getAnchorCountCB(AtkHyperlink *aLink);
G_END_DECLS

static gpointer parent_class = NULL;
static nsAccessible*
get_accessible_hyperlink(AtkHyperlink *aHyperlink);

GType
mai_atk_hyperlink_get_type(void)
{
    static GType type = 0;

    if (!type) {
        static const GTypeInfo tinfo = {
            sizeof(MaiAtkHyperlinkClass),
            (GBaseInitFunc)NULL,
            (GBaseFinalizeFunc)NULL,
            (GClassInitFunc)classInitCB,
            (GClassFinalizeFunc)NULL,
            NULL, /* class data */
            sizeof(MaiAtkHyperlink), /* instance size */
            0, /* nb preallocs */
            (GInstanceInitFunc)NULL,
            NULL /* value table */
        };

        type = g_type_register_static(ATK_TYPE_HYPERLINK,
                                      "MaiAtkHyperlink",
                                      &tinfo, GTypeFlags(0));
    }
    return type;
}

MaiHyperlink::MaiHyperlink(nsAccessible* aHyperLink) :
    mHyperlink(aHyperLink),
    mMaiAtkHyperlink(nsnull)
{
}

MaiHyperlink::~MaiHyperlink()
{
    if (mMaiAtkHyperlink) {
        MAI_ATK_HYPERLINK(mMaiAtkHyperlink)->maiHyperlink = nsnull;
        g_object_unref(mMaiAtkHyperlink);
    }
}

AtkHyperlink*
MaiHyperlink::GetAtkHyperlink(void)
{
  NS_ENSURE_TRUE(mHyperlink, nsnull);

  if (mMaiAtkHyperlink)
    return mMaiAtkHyperlink;

  if (!mHyperlink->IsLink())
    return nsnull;

    mMaiAtkHyperlink =
        reinterpret_cast<AtkHyperlink *>
                        (g_object_new(mai_atk_hyperlink_get_type(), NULL));
    NS_ASSERTION(mMaiAtkHyperlink, "OUT OF MEMORY");
    NS_ENSURE_TRUE(mMaiAtkHyperlink, nsnull);

    /* be sure to initialize it with "this" */
    MaiHyperlink::Initialize(mMaiAtkHyperlink, this);

    return mMaiAtkHyperlink;
}

/* static */

/* remember to call this static function when a MaiAtkHyperlink
 * is created
 */

nsresult
MaiHyperlink::Initialize(AtkHyperlink *aObj, MaiHyperlink *aHyperlink)
{
    NS_ENSURE_ARG(MAI_IS_ATK_HYPERLINK(aObj));
    NS_ENSURE_ARG(aHyperlink);

    /* initialize hyperlink */
    MAI_ATK_HYPERLINK(aObj)->maiHyperlink = aHyperlink;
    return NS_OK;
}

/* static functions for ATK callbacks */

void
classInitCB(AtkHyperlinkClass *aClass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(aClass);

    parent_class = g_type_class_peek_parent(aClass);

    aClass->get_uri = getUriCB;
    aClass->get_object = getObjectCB;
    aClass->get_end_index = getEndIndexCB;
    aClass->get_start_index = getStartIndexCB;
    aClass->is_valid = isValidCB;
    aClass->get_n_anchors = getAnchorCountCB;

    gobject_class->finalize = finalizeCB;
}

void
finalizeCB(GObject *aObj)
{
    NS_ASSERTION(MAI_IS_ATK_HYPERLINK(aObj), "Invalid MaiAtkHyperlink");
    if (!MAI_IS_ATK_HYPERLINK(aObj))
        return;

    MaiAtkHyperlink *maiAtkHyperlink = MAI_ATK_HYPERLINK(aObj);
    maiAtkHyperlink->maiHyperlink = nsnull;

    /* call parent finalize function */
    if (G_OBJECT_CLASS (parent_class)->finalize)
        G_OBJECT_CLASS (parent_class)->finalize(aObj);
}

gchar *
getUriCB(AtkHyperlink *aLink, gint aLinkIndex)
{
    nsAccessible* hyperlink = get_accessible_hyperlink(aLink);
    NS_ENSURE_TRUE(hyperlink, nsnull);

    nsCOMPtr<nsIURI> uri = hyperlink->AnchorURIAt(aLinkIndex);
    if (!uri)
        return nsnull;

    nsCAutoString cautoStr;
    nsresult rv = uri->GetSpec(cautoStr);
    NS_ENSURE_SUCCESS(rv, nsnull);

    return g_strdup(cautoStr.get());
}

AtkObject *
getObjectCB(AtkHyperlink *aLink, gint aLinkIndex)
{
    nsAccessible* hyperlink = get_accessible_hyperlink(aLink);
    NS_ENSURE_TRUE(hyperlink, nsnull);

    nsAccessible* anchor = hyperlink->AnchorAt(aLinkIndex);
    NS_ENSURE_TRUE(anchor, nsnull);

    AtkObject *atkObj = nsAccessibleWrap::GetAtkObject(anchor);
    //no need to add ref it, because it is "get" not "ref"
    return atkObj;
}

gint
getEndIndexCB(AtkHyperlink *aLink)
{
    nsAccessible* hyperlink = get_accessible_hyperlink(aLink);
    NS_ENSURE_TRUE(hyperlink, -1);

    return static_cast<gint>(hyperlink->EndOffset());
}

gint
getStartIndexCB(AtkHyperlink *aLink)
{
    nsAccessible* hyperlink = get_accessible_hyperlink(aLink);
    NS_ENSURE_TRUE(hyperlink, -1);

    return static_cast<gint>(hyperlink->StartOffset());
}

gboolean
isValidCB(AtkHyperlink *aLink)
{
    nsAccessible* hyperlink = get_accessible_hyperlink(aLink);
    NS_ENSURE_TRUE(hyperlink, FALSE);

    return static_cast<gboolean>(hyperlink->IsLinkValid());
}

gint
getAnchorCountCB(AtkHyperlink *aLink)
{
    nsAccessible* hyperlink = get_accessible_hyperlink(aLink);
    NS_ENSURE_TRUE(hyperlink, -1);

    return static_cast<gint>(hyperlink->AnchorCount());
}

// Check if aHyperlink is a valid MaiHyperlink, and return the
// HyperLinkAccessible related.
nsAccessible*
get_accessible_hyperlink(AtkHyperlink *aHyperlink)
{
    NS_ENSURE_TRUE(MAI_IS_ATK_HYPERLINK(aHyperlink), nsnull);
    MaiHyperlink * maiHyperlink =
        MAI_ATK_HYPERLINK(aHyperlink)->maiHyperlink;
    NS_ENSURE_TRUE(maiHyperlink != nsnull, nsnull);
    NS_ENSURE_TRUE(maiHyperlink->GetAtkHyperlink() == aHyperlink, nsnull);
    return maiHyperlink->GetAccHyperlink();
}
