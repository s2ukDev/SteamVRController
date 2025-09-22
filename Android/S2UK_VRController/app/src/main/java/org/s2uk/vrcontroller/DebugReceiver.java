package org.s2uk.vrcontroller;

public class DebugReceiver {

    public static class DecodedPacket {
        public boolean leftController;
        public int triggerState;
        public int gripState;
        public boolean btnSystemOrMenu;
        public boolean btnAorX;
        public boolean btnBorY;
        public boolean batteryPlugged;
        public int joyState;
        public boolean joyInDZ;
        public float batteryPercent;

        public double gx, gy, gz;
        public double jx, jy;

        @Override
        public String toString() {
            return "DecodedPacket{" +
                    "leftController=" + leftController +
                    ", triggerState=" + triggerState +
                    ", gripState=" + gripState +
                    ", btnSystemOrMenu=" + btnSystemOrMenu +
                    ", btnAorX=" + btnAorX +
                    ", btnBorY=" + btnBorY +
                    ", batteryPlugged=" + batteryPlugged +
                    ", joyInDZ=" + joyInDZ +
                    ", joyState=" + joyState +
                    ", batteryPercent=" + batteryPercent +
                    ", gx=" + gx + ", gy=" + gy + ", gz=" + gz +
                    ", jx=" + jx + ", jy=" + jy +
                    '}';
        }
    }

    private static final double GYRO_SCALE = 1000.0;
    private static final double JOY_SCALE  = 100000.0;

    public static DecodedPacket fromBase64(String b64) {
        byte[] raw;
        raw = java.util.Base64.getDecoder().decode(b64);
        return fromBytes(raw);
    }

    public static DecodedPacket fromBytes(byte[] buf) {
        if (buf == null || buf.length < 3) {
            throw new IllegalArgumentException("packet too short");
        }
        int pos = 0;
        int flags = buf[pos++] & 0xFF;
        int modes = buf[pos++] & 0xFF;
        int bat   = buf[pos++] & 0xFF;

        DecodedPacket out = new DecodedPacket();

        out.leftController = (flags & (1 << 0)) != 0;
        out.btnSystemOrMenu = (flags & (1 << 1)) != 0;
        out.btnAorX = (flags & (1 << 2)) != 0;
        out.btnBorY = (flags & (1 << 3)) != 0;
        out.batteryPlugged = (flags & (1 << 4)) != 0;
        out.joyInDZ = (flags & (1 << 5)) != 0;

        out.triggerState = (modes) & 0x3;
        out.gripState = (modes >> 2) & 0x3;
        out.joyState = (modes >> 4) & 0x3;
        out.batteryPercent = (float)bat / 100.0f;

        // Now read 5 varints: gx, gy, gz, jx, jy
        int[] newPos = new int[] { pos };
        long zz;

        zz = readVarUint64(buf, newPos);
        out.gx = (double) zigzagDecode(zz) / GYRO_SCALE;

        zz = readVarUint64(buf, newPos);
        out.gy = (double) zigzagDecode(zz) / GYRO_SCALE;

        zz = readVarUint64(buf, newPos);
        out.gz = (double) zigzagDecode(zz) / GYRO_SCALE;

        zz = readVarUint64(buf, newPos);
        out.jx = (double) zigzagDecode(zz) / JOY_SCALE;

        zz = readVarUint64(buf, newPos);
        out.jy = (double) zigzagDecode(zz) / JOY_SCALE;

        return out;
    }

    private static long readVarUint64(byte[] buf, int[] pos) {
        int p = pos[0];
        long result = 0L;
        int shift = 0;
        while (p < buf.length) {
            int b = buf[p++] & 0xFF;
            result |= (long)(b & 0x7F) << shift;
            if ((b & 0x80) == 0) {
                pos[0] = p;
                return result;
            }
            shift += 7;
            if (shift >= 64) throw new IllegalArgumentException("varint overflow");
        }
        throw new IllegalArgumentException("varint truncated");
    }

    private static long zigzagDecode(long zz) {
        // (zz >>> 1) ^ -(zz & 1)
        return (zz >>> 1) ^ -(zz & 1L);
    }
}