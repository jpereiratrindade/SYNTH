import type { Metadata } from "next";
import { headers } from "next/headers";
import { Geist, Geist_Mono } from "next/font/google";
import "./globals.css";

const sans = Geist({ variable: "--font-sans", subsets: ["latin"] });
const mono = Geist_Mono({ variable: "--font-mono", subsets: ["latin"] });

export async function generateMetadata(): Promise<Metadata> {
  const requestHeaders = await headers();
  const host = requestHeaders.get("host") ?? "localhost:3000";
  const protocol = requestHeaders.get("x-forwarded-proto") ?? (host.startsWith("localhost") ? "http" : "https");
  const origin = `${protocol}://${host}`;
  const title = "SYNTH — Sempre pronto, sempre incompleto";
  const description = "Um sistema aberto em C++26 que observa o que existe sem inventar o que ainda não existe.";
  return {
    metadataBase: new URL(origin), title, description,
    openGraph: { title, description, type: "website", locale: "pt_BR", siteName: "SYNTH", images: [{ url: `${origin}/og.png`, width: 1200, height: 630, alt: "SYNTH — Sistema pronto" }] },
    twitter: { card: "summary_large_image", title, description, images: [`${origin}/og.png`] },
  };
}

export default function RootLayout({ children }: Readonly<{ children: React.ReactNode }>) {
  return <html lang="pt-BR"><body className={`${sans.variable} ${mono.variable}`}>{children}</body></html>;
}
