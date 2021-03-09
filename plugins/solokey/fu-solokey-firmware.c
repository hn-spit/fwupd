/*
 * Copyright (C) 2019 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#include "config.h"

#include <json-glib/json-glib.h>

#include "fu-common.h"
#include "fu-ihex-firmware.h"

#include "fu-solokey-firmware.h"

struct _FuSolokeyFirmware {
	FuFirmware		 parent_instance;
};

G_DEFINE_TYPE (FuSolokeyFirmware, fu_solokey_firmware, FU_TYPE_FIRMWARE)

static GBytes *
_g_base64_decode_to_bytes (const gchar *text)
{
	gsize out_len = 0;
	guchar *out = g_base64_decode (text, &out_len);
	return g_bytes_new_take ((guint8 *) out, out_len);
}

static gboolean
fu_solokey_firmware_parse (FuFirmware *firmware,
			   GBytes *fw,
			   guint64 addr_start,
			   guint64 addr_end,
			   FwupdInstallFlags flags,
			   GError **error)
{
	JsonNode *json_root;
	JsonObject *json_obj;
	const gchar *base64;
	g_autoptr(FuFirmware) ihex_firmware = fu_ihex_firmware_new ();
	g_autoptr(FuFirmware) img_sig = fu_firmware_new ();
	g_autoptr(GBytes) fw_ihex = NULL;
	g_autoptr(GBytes) fw_sig = NULL;
	g_autoptr(GString) base64_websafe = NULL;
	g_autoptr(JsonParser) parser = json_parser_new ();

	/* parse JSON */
	if (!json_parser_load_from_data (parser,
					 (const gchar *) g_bytes_get_data (fw, NULL),
					 (gssize) g_bytes_get_size (fw),
					 error)) {
		g_prefix_error (error, "firmware not in JSON format: ");
		return FALSE;
	}
	json_root = json_parser_get_root (parser);
	if (json_root == NULL || !JSON_NODE_HOLDS_OBJECT (json_root)) {
		g_set_error_literal (error,
				     FWUPD_ERROR,
				     FWUPD_ERROR_INVALID_FILE,
				     "JSON invalid as has no root");
		return FALSE;
	}
	json_obj = json_node_get_object (json_root);
	if (!json_object_has_member (json_obj, "firmware")) {
		g_set_error_literal (error,
				     FWUPD_ERROR,
				     FWUPD_ERROR_INVALID_FILE,
				     "JSON invalid as has no 'firmware'");
		return FALSE;
	}
	if (!json_object_has_member (json_obj, "signature")) {
		g_set_error_literal (error,
				     FWUPD_ERROR,
				     FWUPD_ERROR_INVALID_FILE,
				     "JSON invalid as has no 'signature'");
		return FALSE;
	}

	/* decode */
	base64 = json_object_get_string_member (json_obj, "firmware");
	if (base64 == NULL) {
		g_set_error_literal (error,
				     FWUPD_ERROR,
				     FWUPD_ERROR_INVALID_FILE,
				     "JSON 'firmware' missing");
		return FALSE;
	}
	fw_ihex = _g_base64_decode_to_bytes (base64);
	if (!fu_firmware_parse (ihex_firmware, fw_ihex, flags, error))
		return FALSE;
	fw = fu_firmware_get_bytes (ihex_firmware, error);
	if (fw == NULL)
		return FALSE;
	fu_firmware_set_addr (firmware, fu_firmware_get_addr (ihex_firmware));
	fu_firmware_set_bytes (firmware, fw);

	/* signature */
	base64 = json_object_get_string_member (json_obj, "signature");
	if (base64 == NULL) {
		g_set_error_literal (error,
				     FWUPD_ERROR,
				     FWUPD_ERROR_INVALID_FILE,
				     "JSON 'signature' missing");
		return FALSE;
	}
	base64_websafe = g_string_new (base64);
	fu_common_string_replace (base64_websafe, "-", "+");
	fu_common_string_replace (base64_websafe, "_", "/");
	g_string_append (base64_websafe, "==");
	fw_sig = _g_base64_decode_to_bytes (base64_websafe->str);
	fu_firmware_set_bytes (img_sig, fw_sig);
	fu_firmware_set_id (img_sig, FU_FIRMWARE_ID_SIGNATURE);
	fu_firmware_add_image (firmware, img_sig);
	return TRUE;
}

static void
fu_solokey_firmware_init (FuSolokeyFirmware *self)
{
}

static void
fu_solokey_firmware_class_init (FuSolokeyFirmwareClass *klass)
{
	FuFirmwareClass *klass_firmware = FU_FIRMWARE_CLASS (klass);
	klass_firmware->parse = fu_solokey_firmware_parse;
}

FuFirmware *
fu_solokey_firmware_new (void)
{
	return FU_FIRMWARE (g_object_new (FU_TYPE_SOLOKEY_FIRMWARE, NULL));
}
